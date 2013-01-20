# set the include path for X, qt and KDE
INCLUDES = $(all_includes)

# these are the headers for your project
noinst_HEADERS = kafp.h

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO

messages: rc.cpp
	$(EXTRACTRC) `find . -name \*.ui -o -name \*.rc` > rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kioslave_afp.pot

KDE_ICON = AUTO

#########################################################################
# APPLICATION SECTION
#########################################################################
# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables

# bin_PROGRAMS = afplogintest

# afplogintest_SOURCES = kdeveloptest2.cpp main.cpp afploginwidget.cpp \
# 	afploginwidgetbase.ui

# afplogintest_LDFLAGS = $(KDE_RPATH) $(all_libraries)
# afplogintest_LDADD = $(LIB_KDEUI)

# And now our library

kde_module_LTLIBRARIES = kio_afp.la

kio_afp_la_SOURCES = afploginwidget.cpp afploginwidgetbase.ui kafp.cpp
kio_afp_la_LIBADD = -lafpclient -lkio
kio_afp_la_LDFLAGS = -modules $(KDE_PLUGIN) $(all_libraries) -lafpclient -lafpsl

protocol_DATA = afp.protocol
protocoldir = $(kde_servicesdir)
