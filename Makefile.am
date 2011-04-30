
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DDATADIR='"$(pkgdatadir)"' -I$(srcdir)/src/include
AM_LDFLAGS  = -L$(builddir)/lib

##_LDADD  - programs
##_LIBADD - libraries
# Use these for -l, -L
#
##_LDFLAGS - use for other flags --- overrides $(AM_LDFLAGS)



#####
### Libraries
#####
lib_LTLIBRARIES = lib/libcheetah-common.la lib/libcheetah.la


## libcheetah-common
lib_libcheetah_common_la_LDFLAGS = -shared $(AM_LDFLAGS)
lib_libcheetah_common_la_SOURCES = src/libcheetah-common/cheetah-common.c


## libcheetah
lib_libcheetah_la_LDFLAGS = -shared $(AM_LDFLAGS)
lib_libcheetah_la_LIBADD  = $(builddir)/lib/libcheetah-common.la
lib_libcheetah_la_SOURCES = src/libcheetah/cheetah-api.c


## Installable library headers
pkginclude_HEADERS = src/include/cheetah/cheetah-common.h \
                     src/include/cheetah/cheetah.h


#####
### Programs
#####
bin_PROGRAMS = bin/cheetah-jobscheduler \
               bin/cheetah-pumanager    \
               bin/cheetah-resultscollector


## cheetah-jobscheduler
bin_cheetah_jobscheduler_LDADD   = $(builddir)/lib/libcheetah-common.la
bin_cheetah_jobscheduler_SOURCES =                                             \
                        src/jobscheduler/main.c                                \
                        src/jobscheduler/sched_workload_predictive.c           \
                        src/jobscheduler/sched_dispatch_FCFS.c                 \
                        src/jobscheduler/sched_workload_rr.c                   \
                        src/jobscheduler/schedUtils.c                          \
                        src/jobscheduler/sched_workload_weight_devthroughput.c \
                        src/jobscheduler/scheduler.h

#noinst_HEADERS=scheduler.h
#From the GNU Automake manual, version 1.11.1, 8 December 2009:
#"when the header actually belongs to a single convenience library or program,
#we recommend listing it in the program's or library's _SOURCES variable (see
#Section 8.1.1 [Program Sources], page 52) instead of in noinst_HEADERS. This is
#clearer for the Makefile.am reader. noinst_HEADERS would be the right variable
#to use in a directory containing only headers and no associated library or
#program."


## cheetah-pumanager
bin_cheetah_pumanager_LDADD  = $(builddir)/lib/libcheetah-common.la
bin_cheetah_pumanager_SOURCES = src/pumanager/main.c        \
                                src/pumanager/info.c        \
                                src/pumanager/pumUtils.c    \
                                src/pumanager/testKernels.c \
                                src/pumanager/pum.h

pkgdata_pumanagerdir        = $(pkgdatadir)/pumanager
dist_pkgdata_pumanager_DATA = src/pumanager/mandel.cl             \
                              src/pumanager/setupBandwidthTest.cl \
                              src/pumanager/setupLowProcTest.cl


## cheetah-resultscollector
bin_cheetah_resultscollector_LDADD   = $(builddir)/lib/libcheetah-common.la
bin_cheetah_resultscollector_SOURCES = src/resultscollector/main.c        \
                                       src/resultscollector/rescolUtils.c \
                                       src/resultscollector/rc.h



distclean-local:
	rm -fr ./*~ ./autom4te.cache/ ./build/* ./m4/* ./*.in aclocal.m4 ./configure
