NULL=

desktopdir       = $(datadir)/applications
desktop_in_files = 					\
	org.gnome.Nautilus.desktop.in			\
	nautilus-autorun-software.desktop.in

desktop_DATA     = 					\
	$(desktop_in_files:.desktop.in=.desktop)

autostart_in_files = nautilus-autostart.desktop.in

if ENABLE_DESKTOP
desktop_DATA += nautilus-classic.desktop

autostartdir     = $(sysconfdir)/xdg/autostart
autostart_DATA   = $(autostart_in_files:.desktop.in=.desktop)
endif

appdatadir = $(datadir)/appdata
appdata_in_files = org.gnome.Nautilus.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

@INTLTOOL_DESKTOP_RULE@

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.freedesktop.FileManager1.service.in	\
	org.gnome.Nautilus.service.in

org.gnome.Nautilus.service: org.gnome.Nautilus.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

org.freedesktop.FileManager1.service: org.freedesktop.FileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = nautilus-search-provider.ini

gsettings_SCHEMAS = org.gnome.nautilus.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST =				\
	org.gnome.Nautilus.desktop	\
	org.gnome.Nautilus.desktop.in	\
	dbus-interfaces.xml		\
	freedesktop-dbus-interfaces.xml	\
	shell-search-provider-dbus-interfaces.xml \
	$(gsettings_SCHEMAS)     \
	$(desktop_in_files)		\
	$(service_in_files)		\
	$(autostart_in_files)		\
	$(searchprovider_DATA)		\
	$(appdata_in_files)		\
	$(NULL)

CLEANFILES =			\
	$(desktop_DATA)		\
	$(service_DATA)		\
	$(appdata_DATA)		\
	$(NULL)

if ENABLE_DESKTOP
CLEANFILES += $(autostart_DATA)
endif


-include $(top_srcdir)/git.mk
