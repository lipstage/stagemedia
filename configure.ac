AC_INIT(stagemedia, 0.8, fralick@gmail.com)
AC_PROG_CC

#
# To cheat: for i in $(grep 'ac_cv_header_' config.log|awk -F= '{print $1}' ); do echo  'test "$'$i'" == "no" || \'; done; echo ""
#
AC_CHECK_HEADERS(arpa/inet.h)
AC_CHECK_HEADERS(ctype.h)
AC_CHECK_HEADERS(curl/curl.h)
AC_CHECK_HEADERS(errno.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(jansson.h)
AC_CHECK_HEADERS(lame/lame.h)
AC_CHECK_HEADERS(math.h)
AC_CHECK_HEADERS(netdb.h)
AC_CHECK_HEADERS(netinet/in.h)
AC_CHECK_HEADERS(pthread.h)
AC_CHECK_HEADERS(signal.h)
AC_CHECK_HEADERS(sndfile.h)
AC_CHECK_HEADERS(stdio.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(sys/select.h)
AC_CHECK_HEADERS(sys/socket.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/wait.h)
AC_CHECK_HEADERS(unistd.h)
if test "$ac_cv_header_arpa_inet_h" == "no" || \
 test "$ac_cv_header_ctype_h" == "no" || \
 test "$ac_cv_header_curl_curl_h" == "no" || \
 test "$ac_cv_header_errno_h" == "no" || \
 test "$ac_cv_header_fcntl_h" == "no" || \
 test "$ac_cv_header_inttypes_h" == "no" || \
 test "$ac_cv_header_jansson_h" == "no" || \
 test "$ac_cv_header_lame_lame_h" == "no" || \
 test "$ac_cv_header_math_h" == "no" || \
 test "$ac_cv_header_memory_h" == "no" || \
 test "$ac_cv_header_netdb_h" == "no" || \
 test "$ac_cv_header_netinet_in_h" == "no" || \
 test "$ac_cv_header_pthread_h" == "no" || \
 test "$ac_cv_header_signal_h" == "no" || \
 test "$ac_cv_header_sndfile_h" == "no" || \
 test "$ac_cv_header_stdc" == "no" || \
 test "$ac_cv_header_stdint_h" == "no" || \
 test "$ac_cv_header_stdio_h" == "no" || \
 test "$ac_cv_header_stdlib_h" == "no" || \
 test "$ac_cv_header_string_h" == "no" || \
 test "$ac_cv_header_strings_h" == "no" || \
 test "$ac_cv_header_sys_select_h" == "no" || \
 test "$ac_cv_header_sys_socket_h" == "no" || \
 test "$ac_cv_header_sys_stat_h" == "no" || \
 test "$ac_cv_header_sys_time_h" == "no" || \
 test "$ac_cv_header_sys_types_h" == "no" || \
 test "$ac_cv_header_sys_wait_h" == "no" || \
 test "$ac_cv_header_unistd_h" == "no" 
then
 AC_MSG_ERROR([You are missing one or more required headers -- consider installing devel packages])
fi

#
# To cheat: for i in $(grep 'ac_cv_lib_' config.log|awk -F= '{print $1}' ); do echo  'test "$'$i'" == "no" || \'; done; echo ""
#
AC_CHECK_LIB(pthread, pthread_mutex_init)
AC_CHECK_LIB(mp3lame, lame_init)
AC_CHECK_LIB(curl, curl_easy_init)
AC_CHECK_LIB(m, cos)
AC_CHECK_LIB(jansson, getpid)
if test "$ac_cv_lib_curl_curl_easy_init" == "no" || \
 test "$ac_cv_lib_jansson_getpid" == "no" || \
 test "$ac_cv_lib_m_cos" == "no" || \
 test "$ac_cv_lib_mp3lame_lame_init" == "no" || \
 test "$ac_cv_lib_pthread_pthread_mutex_init" == "no" 
then
 AC_MSG_ERROR([You are missing one or more required libraries])
fi

AC_MSG_CHECKING([whether CC supports -fomit-frame-pointer])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
 [AC_MSG_RESULT([yes])]
 [FOMIT_FRAME_POINTER=-fomit-frame-pointer],
 [AC_MSG_RESULT([no])]
)
AC_SUBST([FOMIT_FRAME_POINTER])
AC_MSG_CHECKING([whether CC supports -Wall])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
 [AC_MSG_RESULT([yes])]
 [WALL=-Wall],
 [AC_MSG_RESULT([no])]
)
AC_SUBST([WALL])

#LDFLAGS="$LDFLAGS -lpthread -lmp3lame -lcurl -lm"
AC_OUTPUT(Makefile)
