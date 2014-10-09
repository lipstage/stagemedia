AC_INIT(stagemedia, 0.8, fralick@gmail.com)
AC_PROG_CC

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

AC_CHECK_LIB(pthread, pthread_mutex_init)
AC_CHECK_LIB(mp3lame, lame_init)
AC_CHECK_LIB(curl, curl_easy_init)
AC_CHECK_LIB(m, cos)
AC_CHECK_LIB(jansson, getpid)

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
