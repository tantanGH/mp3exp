# この makefile は、msys や cygwin などの Unix 互換環境上で利用することを想定している。
# ビルドには xdev68k が必要。
# https://github.com/yosshin4004/xdev68k

# 必要な環境変数が定義されていることを確認する。
ifndef XDEV68K_DIR
	$(error ERROR : XDEV68K_DIR is not defined.)
endif

# デフォルトサフィックスを削除
.SUFFIXES:

# 各種コマンド短縮名
CXX = ${XDEV68K_DIR}/m68k-toolchain/bin/m68k-elf-g++
CC = ${XDEV68K_DIR}/m68k-toolchain/bin/m68k-elf-gcc
GAS2HAS = perl ${XDEV68K_DIR}/util/x68k_gas2has.pl -cpu 68000 -inc doscall.inc -inc iocscall.inc
RUN68 = ${XDEV68K_DIR}/run68/run68
HAS = $(RUN68) ${XDEV68K_DIR}/x68k_bin/HAS060.X
#HLK = $(RUN68) ${XDEV68K_DIR}/x68k_bin/hlk301.x
HLK = $(RUN68) ${XDEV68K_DIR}/x68k_bin/LK.X

# 実行ファイル名
TARGET_FILE = MP3EXP.X

# Document file
DOCUMENT_FILE = MP3EXP.DOC

# Distribution package 
PACKAGE_FILE = ../MPEXP076.ZIP

# ヘッダ検索パス
INCLUDE_FLAGS = -I${XDEV68K_DIR}/include/xc -I${XDEV68K_DIR}/include/xdev68k -I${XDEV68K_DIR}/include/mad -I${XDEV68K_DIR}/include/zlib

# コンパイルフラグ
COMMON_FLAGS = -m68000 -Os $(INCLUDE_FLAGS)
#COMMON_FLAGS = -m68060 -msoft-float -Os $(INCLUDE_FLAGS)
CFLAGS = $(COMMON_FLAGS) -fno-conserve-stack -Wno-builtin-declaration-mismatch -fcall-used-d2 \
				-finput-charset=cp932 -fexec-charset=cp932 -fcall-used-a2 -fverbose-asm \
				-D_TIME_T_DECLARED -D_CLOCK_T_DECLARED

# *.c ソースファイル
C_SRCS = crtc.c himem.c nanojpeg.c buffer.c png.c pcm8.c pcm8a.c pcm8pp.c adpcm.c nas_adpcm.c mp3.c main.c

# *.s ソースファイル
ASM_SRCS = utf16_cp932.s nas_adpcmlib.s

# *.h source files
HEADER_SRCS = keyboard.h utf16_cp932.h nas_macro.h crtc.h himem.h nanojpeg.h buffer.h png.h pcm8.h pcm8a.h pcm8pp.h adpcm.h nas_adpcm.h mp3.h mp3exp.h

# リンク対象のライブラリファイル
# link custom libmad objects compiled with OPT_SPEED option instead of normal libmad.a
LIBS =\
	${XDEV68K_DIR}/lib/xc/CLIB.L \
	${XDEV68K_DIR}/lib/xc/DOSLIB.L \
	${XDEV68K_DIR}/lib/xc/IOCSLIB.L \
	${XDEV68K_DIR}/lib/xc/FLOATFNC.L \
	${XDEV68K_DIR}/lib/mad/libmad-optspeed.a \
	${XDEV68K_DIR}/lib/zlib/libz.a \
	${XDEV68K_DIR}/lib/m68k_elf/m68000/libgcc.a

#	${XDEV68K_DIR}/lib/mad/libmad-optspeed.a \

# 中間ファイル生成用ディレクトリ
INTERMEDIATE_DIR = _build

# オブジェクトファイル
OBJS =	$(addprefix $(INTERMEDIATE_DIR)/,$(patsubst %.c,%.o,$(C_SRCS))) \
	$(addprefix $(INTERMEDIATE_DIR)/,$(patsubst %.s,%.o,$(ASM_SRCS)))

# HLK に入力するリンクリスト
HLK_LINK_LIST = $(INTERMEDIATE_DIR)/_lk_list.tmp

# デフォルトのターゲット
all : ${INTERMEDIATE_DIR}/$(TARGET_FILE)

# 中間生成物の削除
clean : 
	rm -f $(TARGET_FILE)
	rm -rf $(INTERMEDIATE_DIR)

# 実行ファイルの生成
#	HLK に長いパス文字を与えることは難しい。
#	回避策としてリンク対象ファイルを $(INTERMEDIATE_DIR) 以下にコピーし、
#	短い相対パスを用いてリンクを実行させる。
${INTERMEDIATE_DIR}/$(TARGET_FILE) : $(OBJS)
	mkdir -p $(INTERMEDIATE_DIR)
	rm -f $(HLK_LINK_LIST)
	@for FILENAME in $(OBJS); do\
		echo $$FILENAME >> $(HLK_LINK_LIST); \
        done
	@for FILENAME in $(LIBS); do\
		cp $$FILENAME $(INTERMEDIATE_DIR)/`basename $$FILENAME`; \
		echo $(INTERMEDIATE_DIR)/`basename $$FILENAME` >> $(HLK_LINK_LIST); \
        done
	$(HLK) -i $(HLK_LINK_LIST) -o ${INTERMEDIATE_DIR}/$(TARGET_FILE)
	rm -f tmp*.\$$\$$\$$

# *.c ソースのコンパイル
$(INTERMEDIATE_DIR)/%.o : %.c Makefile $(HEADER_SRCS)
	mkdir -p $(INTERMEDIATE_DIR)
	$(CC) -S $(CFLAGS) -o $(INTERMEDIATE_DIR)/$*.m68k-gas.s $<
	$(GAS2HAS) -i $(INTERMEDIATE_DIR)/$*.m68k-gas.s -o $(INTERMEDIATE_DIR)/$*.s
	rm -f $(INTERMEDIATE_DIR)/$*.m68k-gas.s
	$(HAS) -e -u -w0 $(INCLUDE_FLAGS) $(INTERMEDIATE_DIR)/$*.s -o $(INTERMEDIATE_DIR)/$*.o

# *.s ソースのアセンブル
$(INTERMEDIATE_DIR)/%.o : %.s Makefile ${HEADER_SRCS}
	mkdir -p $(INTERMEDIATE_DIR)
	$(HAS) -e -u -w0 $(INCLUDE_FLAGS) $*.s -o $(INTERMEDIATE_DIR)/$*.o

package:
	zip -j ${PACKAGE_FILE} ${INTERMEDIATE_DIR}/${TARGET_FILE} ${DOCUMENT_FILE} 
