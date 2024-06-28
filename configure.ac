AC_INIT([basic_alsa2], [1.0], [alexandru.toader.ext@siemens.com])
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_CHECK_HEADERS([alsa/asoundlib.h], [], [AC_MSG_ERROR([Could not find alsa/asoundlib.h])])
AC_CHECK_LIB([asound], [snd_pcm_open], [], [AC_MSG_ERROR([Could not find libasound])])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT