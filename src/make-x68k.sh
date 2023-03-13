#!/bin/bash

if [ "${XDEV68K_DIR}" == "" ]; then
  echo "error: XDEV68K_DIR environment variable is not defined."
  exit 1
fi

WORKING_DIR=`pwd`
LIBMAD_DIR="${WORKING_DIR}/../libmad-0.15.1b"

TARGET_FILE="MP3EXP.X"
DOC_FILE="../MP3EXP.DOC"
ZIP_FILE="../../MPEXP094.ZIP"

CC=${XDEV68K_DIR}/m68k-toolchain/bin/m68k-elf-gcc
GAS2HAS="${XDEV68K_DIR}/util/x68k_gas2has.pl -cpu 68000 -inc doscall.inc -inc iocscall.inc"
RUN68=${XDEV68K_DIR}/run68/run68
HAS=${XDEV68K_DIR}/x68k_bin/HAS060.X
#HLK=${XDEV68K_DIR}/x68k_bin/hlk301.x
HLK=${XDEV68K_DIR}/x68k_bin/LK.X
HLK_LINK_LIST=_lk_list.tmp

INCLUDE_FLAGS="-I${XDEV68K_DIR}/include/xc -I${XDEV68K_DIR}/include/xdev68k -I${XDEV68K_DIR}/include/zlib -I${LIBMAD_DIR}"
COMMON_FLAGS="-m68000 -Os ${INCLUDE_FLAGS} -z-stack=32768"
CFLAGS="${COMMON_FLAGS} -Wno-builtin-declaration-mismatch -fcall-used-d2 -fcall-used-a2 \
    -fexec-charset=cp932 -fverbose-asm -fno-defer-pop -DFPM_DEFAULT -D_TIME_T_DECLARED -D_CLOCK_T_DECLARED -Dwint_t=int \
		-DXDEV68K"

LIBS="${XDEV68K_DIR}/lib/xc/CLIB.L ${XDEV68K_DIR}/lib/xc/DOSLIB.L ${XDEV68K_DIR}/lib/xc/IOCSLIB.L ${XDEV68K_DIR}/lib/xc/FLOATFNC.L \
    ${XDEV68K_DIR}/lib/zlib/libz.a ${XDEV68K_DIR}/lib/m68k_elf/m68000/libgcc.a"

function do_compile() {
  pushd .
  cd $1
  rm -rf _build
  mkdir -p _build
  for c in $2; do
    echo "compiling ${c}.c in ${1}"
	  ${CC} -S ${CFLAGS} -o _build/${c}.m68k-gas.s ${c}.c
    if [ ! -f _build/${c}.m68k-gas.s ]; then
      return 1
    fi
	  perl ${GAS2HAS} -i _build/${c}.m68k-gas.s -o _build/${c}.s
	  rm -f _build/${c}.m68k-gas.s
	  ${XDEV68K_DIR}/run68/run68 ${HAS} -e -u -w0 ${INCLUDE_FLAGS} _build/${c}.s -o _build/${c}.o
    if [ ! -f _build/${c}.o ]; then
      return 1
    fi
  done
  for c in $3; do
    echo "assembling ${c}.s in ${1}"
	  ${XDEV68K_DIR}/run68/run68 ${HAS} -e -u -w0 ${INCLUDE_FLAGS} ${c}.s -o _build/${c}.o
    if [ ! -f _build/${c}.o ]; then
      return 1
    fi
  done
  popd
  return 0
}

function build_mp3exp() {
  do_compile . "crtc himem nanojpeg png_buffer png pcm8 pcm8a pcm8pp adpcm_encode raw_decode ym2608_decode wav_decode mp3_decode kmd main" "utf16_cp932 ym2608_adpcmlib"
#  do_compile . "png adpcm_encode raw_decode ym2608_decode wav_decode mp3_decode kmd main" ""

  if [ $? != 0 ]; then
    return $?
  fi
  cp -p ${LIBMAD_DIR}/_build/*.o _build/
  cd _build
	rm -f ${HLK_LINK_LIST}
  for a in ${LIBS}; do
    cp -p $a .
  done
	for a in `ls *.o *.L *.a`; do
		echo $a >> ${HLK_LINK_LIST}
  done
  ${XDEV68K_DIR}/run68/run68 ${HLK} -i ${HLK_LINK_LIST} -o ${TARGET_FILE}
  rm -f tmp*.\$$\$$\$$
  zip -jr ${ZIP_FILE} ${DOC_FILE} ${TARGET_FILE}
  cd ..
  return 0
}

build_mp3exp
