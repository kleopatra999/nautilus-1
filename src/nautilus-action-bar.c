/* nautilus-action-bar.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-action-bar.h"
#include "nautilus-clipboard.h"
#include "nautilus-clipboard-monitor.h"
#include "nautilus-file.h"
#include "nautilus-previewer.h"

#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#define               UPDATE_STATUS_TIMEOUT  200 //ms

typedef enum
{
  MODE_NO_SELECTION,
  MODE_FILES_ONLY,
  MODE_FOLDERS_ONLY,
  MODE_MIXED
} ActionBarMode;

struct _NautilusActionBar
{
  GtkFrame            parent;

  GtkWidget          *loading_label;
  GtkWidget          *stack;

  /* No selection buttons */
  GtkWidget          *new_folder_0_button;
  GtkWidget          *paste_button;
  GtkWidget          *select_all_button;
  GtkWidget          *no_selection_separator;
  GtkWidget          *bookmark_button;
  GtkWidget          *properties_0_button;
  GtkWidget          *no_selection_overflow_button;
  GtkWidget          *no_selection_folder_label;

  /* Folders buttons */
  GtkWidget          *open_file_box;
  GtkWidget          *open_folders_button;
  GtkWidget          *move_folders_button;
  GtkWidget          *move_trash_folders_button;
  GtkWidget          *copy_folders_button;
  GtkWidget          *rename_folders_button;
  GtkWidget          *folders_overflow_button;

  GtkWidget          *no_selection_widgets    [5];
  GtkWidget          *files_folders_widgets   [6];
  GtkWidget          *mixed_selection_widgets [5];

  NautilusView       *view;
  ActionBarMode       mode;
  gint                update_status_timeout_id;
};

G_DEFINE_TYPE (NautilusActionBar, nautilus_action_bar, GTK_TYPE_FRAME)

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

static void
update_paste_button (NautilusActionBar *self)
{
  NautilusClipboardMonitor *monitor;
  NautilusClipboardInfo *info;

  monitor = nautilus_clipboard_monitor_get ();
  info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

  gtk_widget_set_visible (self->paste_button, info != NULL);

  if (info)
    {
      gchar *label;
      gint length;

      length = g_list_length (info->files);

      if (info->cut)
        label = g_strdup_printf (g_dngettext(NULL, "Move %d file", "Move %d files", length), length);
      else
        label = g_strdup_printf (g_dngettext(NULL, "Paste %d file", "Paste %d files", length), length);

      gtk_button_set_label (GTK_BUTTON (self->paste_button), label);

      g_free (label);
    }
}

static void
set_internal_mode (NautilusActionBar *self,
                   ActionBarMode      mode)
{
  self->mode = mode;

  switch (mode)
    {
    case MODE_NO_SELECTION:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "no-selection");
      break;

    case MODE_FILES_ONLY:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      break;

    case MODE_FOLDERS_ONLY:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      break;

    case MODE_MIXED:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      break;
    }
}

static gboolean
real_update_status (gpointer data)
{
  NautilusActionBar *actionbar = data;

  if (nautilus_view_is_loading (actionbar->view))
    {
      gtk_label_set_label (GTK_LABEL (actionbar->loading_label),
                           nautilus_view_is_searching (actionbar->view) ? _("Searching") : _("Loading"));

      gtk_stack_set_visible_child_name (GTK_STACK (actionbar->stack), "loading");
    }
  else
    {
      GList *selection, *l;
      gint number_of_files, number_of_folders;

      selection = nautilus_view_get_selection (actionbar->view);
      number_of_files = number_of_folders = 0;

      /* Count the number of selected files and folders */
      for (l = selection; l != NULL; l = l->next)
        {
          if (nautilus_file_is_directory (l->data))
            number_of_folders++;
          else
            number_of_files++;
        }

      if (number_of_files > 0 && number_of_folders > 0)
        set_internal_mode (actionbar, MODE_MIXED);
      else if (number_of_files > 0)
        set_internal_mode (actionbar, MODE_FILES_ONLY);
      else if (number_of_folders > 0)
        set_internal_mode (actionbar, MODE_FOLDERS_ONLY);
      else
        set_internal_mode (actionbar, MODE_NO_SELECTION);
    }

  actionbar->update_status_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
update_status (NautilusActionBar *actionbar)
{
  if (actionbar->update_status_timeout_id > 0)
    {
      g_source_remove (actionbar->update_status_timeout_id);
      actionbar->update_status_timeout_id = 0;
    }

  actionbar->update_status_timeout_id = g_timeout_add (UPDATE_STATUS_TIMEOUT,
                                                       real_update_status,
                                                       actionbar);
}

static void
location_changed_cb (NautilusActionBar *self)
{
  NautilusFile *file;
  gchar *display_name;

  file = nautilus_file_get (nautilus_view_get_location (self->view));

  if (nautilus_file_is_home (file))
    display_name = g_strdup (_("Home"));
  else
    display_name = nautilus_file_get_display_name (file);

  gtk_label_set_label (GTK_LABEL (self->no_selection_folder_label), display_name);

  g_clear_pointer (&file, nautilus_file_unref);
  g_free (display_name);
}

static void
nautilus_action_bar_finalize (GObject *object)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  if (self->update_status_timeout_id > 0)
    {
      g_source_remove (self->update_status_timeout_id);
      self->update_status_timeout_id = 0;
    }

  g_signal_handlers_disconnect_by_func (nautilus_clipboard_monitor_get (), update_paste_button, self);
  g_signal_handlers_disconnect_by_func (self->view, update_status, self);

  g_clear_object (&self->view);

  G_OBJECT_CLASS (nautilus_action_bar_parent_class)->finalize (object);
}

static void
nautilus_action_bar_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_action_bar_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      if (g_set_object (&self->view, g_value_get_object (value)))
        {
          g_signal_connect_swapped (self->view, "notify::location", G_CALLBACK (location_changed_cb), self);
          g_signal_connect_swapped (self->view, "notify::selection", G_CALLBACK (update_status), self);
          g_signal_connect_swapped (self->view, "notify::is-loading", G_CALLBACK (update_status), self);
          g_signal_connect_swapped (self->view, "notify::is-searching", G_CALLBACK (update_status), self);
          g_object_notify (object, "view");
        }

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_action_bar_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  NautilusActionBar *self;
  GtkWidget **widgets;
  GtkWidget *overflow_button;
  gint max_items, visible_items, button_width, overflow_width;
  gint max_width;
  gint i, start;

  self = NAUTILUS_ACTION_BAR (widget);
  max_width = 2 * allocation->width / 3 - 2 * gtk_container_get_border_width (GTK_CONTAINER (self->stack));
  start = 0;

  switch (self->mode)
    {
    case MODE_NO_SELECTION:
      overflow_button = self->no_selection_overflow_button;
      widgets = self->no_selection_widgets;
      max_items = 5;

      gtk_widget_get_preferred_width (self->new_folder_0_button, &button_width, NULL);
      gtk_widget_get_preferred_width (overflow_button, &overflow_width, NULL);

      visible_items = CLAMP ((max_width - overflow_width) / button_width, 0, max_items);

      /* Hide the separator if needed */
      gtk_widget_set_visible (self->no_selection_separator, visible_items > 3);
      break;

    case MODE_FILES_ONLY:
      overflow_button = self->folders_overflow_button;
      widgets = self->files_folders_widgets;
      max_items = 6;
      start = 2;

      gtk_widget_get_preferred_width (self->move_folders_button, &button_width, NULL);

      visible_items = CLAMP (max_width / button_width, 0, max_items);

      gtk_widget_show (widgets[0]);
      gtk_widget_hide (widgets[1]);
      break;

    case MODE_FOLDERS_ONLY:
      overflow_button = self->folders_overflow_button;
      widgets = self->files_folders_widgets;
      max_items = 6;
      start = 1;

      gtk_widget_get_preferred_width (self->move_folders_button, &button_width, NULL);

      visible_items = CLAMP (max_width / button_width, 0, max_items);
      gtk_widget_hide (widgets[0]);
      break;

    case MODE_MIXED:
      overflow_button = self->folders_overflow_button;
      widgets = self->files_folders_widgets;
      max_items = 6;
      start = 2;

      gtk_widget_get_preferred_width (self->move_folders_button, &button_width, NULL);

      visible_items = CLAMP (max_width / button_width, 0, max_items);

      gtk_widget_hide (widgets[0]);
      gtk_widget_hide (widgets[1]);
      break;
    }

  for (i = start; i < max_items; i++)
    {
      g_message ("widget %p is visible: %d", widgets[i], i < visible_items);
      gtk_widget_set_visible (widgets[i], i < visible_items);
    }

  gtk_widget_set_visible (overflow_button, visible_items < max_items);

  GTK_WIDGET_CLASS (nautilus_action_bar_parent_class)->size_allocate (widget, allocation);
}

static void
nautilus_action_bar_get_preferred_width (GtkWidget *widget,
                                         gint      *minimum,
                                         gint      *natural)
{
  NautilusActionBar *self;
  gint button_width;

  self = NAUTILUS_ACTION_BAR (widget);

  switch (self->mode)
    {
    case MODE_NO_SELECTION:
      gtk_widget_get_preferred_width (self->new_folder_0_button, &button_width, NULL);
      break;

    case MODE_FILES_ONLY:
      break;

    case MODE_FOLDERS_ONLY:
      gtk_widget_get_preferred_width (self->open_folders_button, &button_width, NULL);
      break;

    case MODE_MIXED:
      break;
    }

  button_width += 2 * gtk_container_get_border_width (GTK_CONTAINER (self->stack));

  if (minimum)
    *minimum = button_width;

  if (natural)
    *natural = button_width;

  g_message ("%s\t%d", G_STRFUNC, button_width);
}

static void
nautilus_action_bar_class_init (NautilusActionBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nautilus_action_bar_finalize;
  object_class->get_property = nautilus_action_bar_get_property;
  object_class->set_property = nautilus_action_bar_set_property;

  widget_class->size_allocate = nautilus_action_bar_size_allocate;
  widget_class->get_preferred_width = nautilus_action_bar_get_preferred_width;

  /**
   * NautilusActionBar::view:
   *
   * The view related to this actionbar.
   */
  g_object_class_install_property (object_class,
                                   PROP_VIEW,
                                   g_param_spec_object ("view",
                                                        "View of the actionbar",
                                                        "The view related to this actionbar",
                                                        NAUTILUS_TYPE_VIEW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-action-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, bookmark_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, copy_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, loading_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, folders_overflow_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, move_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, move_trash_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, new_folder_0_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, no_selection_folder_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, no_selection_overflow_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, no_selection_separator);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, open_file_box);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, open_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, paste_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, properties_0_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, rename_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, select_all_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, stack);

  gtk_widget_class_set_css_name (widget_class, "actionbar");
}

static void
nautilus_action_bar_init (NautilusActionBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* No selection widgets */
  self->no_selection_widgets[0] = self->new_folder_0_button;
  self->no_selection_widgets[1] = self->paste_button;
  self->no_selection_widgets[2] = self->select_all_button;
  self->no_selection_widgets[3] = self->bookmark_button;
  self->no_selection_widgets[4] = self->properties_0_button;

  /* Folder- and folder-only widgets */
  self->files_folders_widgets[0] = self->open_file_box;
  self->files_folders_widgets[1] = self->open_folders_button;
  self->files_folders_widgets[2] = self->move_folders_button;
  self->files_folders_widgets[3] = self->copy_folders_button;
  self->files_folders_widgets[4] = self->rename_folders_button;
  self->files_folders_widgets[5] = self->move_trash_folders_button;

  update_paste_button (self);

  g_signal_connect_swapped (nautilus_clipboard_monitor_get (), "clipboard-changed",
                            G_CALLBACK (update_paste_button), self);
}

/**
 * nautilus_action_bar_new:
 * @view: a #NautilusView
 *
 * Creates a new actionbar related to @view.
 *
 * Returns: (transfer full): a #NautilusActionBar
 */
GtkWidget*
nautilus_action_bar_new (NautilusView *view)
{
  return g_object_new (NAUTILUS_TYPE_ACTION_BAR,
                       "view", view,
                       NULL);
}
