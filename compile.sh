#!/bin/sh

# In order to generate files somewhere else
# you can cd into target dir and execute this script

panic(){
  echo "${*}" >&2
  exit 1
}

has_flag(){
  v="$(eval 'echo $'${1} |tr '[:upper:]' '[:lower:]')"
  test -n "${v}" && test "${v}" = on
}

SCRIPT_ROOT="$(cd $(dirname $0); echo $PWD)"
SRC_DIR="${SCRIPT_ROOT}/src"
TOOL_DIR="${SCRIPT_ROOT}/tool"

test -z "${HOSTCC}" && HOSTCC=cc
test -z "${CC}" && CC="${HOSTCC}"
test -z "${OUT}" && OUT=portal

CFLAGS="-O0 ${CFLAGS}"
LDFLAGS="${LDFLAGS}"

if test -z "${OS}"; then
  sys="$(uname -s)"
  case "${sys}" in
    Linux*) OS=linux ;;
    Darwin*) OS=apple ;;
    CYGWIN*) OS=cygwin ;;
    *) panic "unknown operating system: ${sys}, set envar OS to [linux|cygwin] and try again" ;;
  esac
fi

CYGWIN=n
if test "${OS}" = cygwin; then
  OS=windows
  CYGWIN=y
fi
filepath(){
  test "${CYGWIN}" = y && echo "$(cygpath -m ${1})" || echo "${1}"
}
# exepath(){
#   test "${OS}" = windows && test "${1##*.}" = "exe" && filepath "${1}" || filepath "${1}.exe"
# }

add_cflags(){
  CFLAGS="${CFLAGS} ${*}"
}

add_ldflags(){
  LDFLAGS="${LDFLAGS} ${*}"
}

if test "${OS}" = linux ; then
  test -z "${PLATFORM_X11}" && PLATFORM_X11=on
  test -z "${PLATFORM_FBDEV}" && PLATFORM_FBDEV=on

  has_flag PLATFORM_X11 &&
    { add_cflags "-DLIBPORTAL_PLATFORM_HAS_X11"; add_ldflags "-lX11 -lXext"; }
  has_flag PLATFORM_FBDEV && add_cflags "-DLIBPORTAL_PLATFORM_HAS_FBDEV"
elif test "${OS}" = windows ; then
  test -z "${PLATFORM_GDI}" && PLATFORM_GDI=on

  has_flag PLATFORM_GDI && {
    add_cflags "-DLIBPORTAL_PLATFORM_HAS_GDI"; add_ldflags "-lws2_32 -lgdi32"; }
else
  panic "unknown os detected: ${OS}"
fi

has_flag ENABLE_GUI && add_cflags "-DPORTAL_ENABLE_GUI"

# compile!

${HOSTCC} -o bin2c "$(filepath ${TOOL_DIR}/bin2c.c)" || panic "cannot compile bin2c"

"$(filepath ./bin2c)" -o res_indexhtml.h -n indexhtml "$(filepath ${SRC_DIR}/index.html)" ||
  panic "cannot generate resource from index.html"

echo "CFLAGS: ${CFLAGS}"
echo "LDFLAGS: ${LDFLAGS}"

${CC} -o "${OUT}" -I. ${CFLAGS} "$(filepath ${SRC_DIR}/main.c)" ${LDFLAGS} || panic "compilation failed"

