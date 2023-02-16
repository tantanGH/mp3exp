# MP3EXP.X
ADPCM/PCM/MP3 player for X680x0/Human68k

以下の形式のファイルを内蔵ADPCMを使って再生します。

 - (PCM) X68K(MSM6258V) ADPCM 15.6kHz mono
 - (S32/S44/S48/M32/M44/M48) 16bit符号付き big endian raw PCM
 - (MP3) MP3

注意：PCMファイルの再生はRed Zone (68000 24MHz)程度のMPUパワーが最低限必要です。

注意：MP3ファイルの再生は060turboとハイメモリがほぼ必須です。オマケです。

注意：このプログラムはコンバータではありません。MP3形式ファイルを ADPCM/PCM 形式に変換するには

[MP3EX.X](https://github.com/tantanGH/mp3ex)

の方を使ってください。

---

### Install

MPEXPxxx.ZIP をダウンロードして展開し、MP3EXP.X をパスの通ったディレクトリに置きます。

---

### How to use

引数をつけずに実行するか、`-h` オプションをつけて実行するとヘルプメッセージが表示されます。

    usage: mp3exp [options] <input-file[.pcm|.s32|.s44|.s48|.m32|.m44|.m48|.mp3]>
    options:
         -a    ... do not use PCM8A.X for ADPCM encoding
         -b<n> ... buffer size [x 64KB] (2-32,default:4)
         -u    ... use 060turbo high memory for buffering
         -l[n] ... loop count (none:infinite, default:1)
         -h    ... show help message

デフォルトでは 12bit PCM から 4bit ADPCMのエンコードに philly さんの PCM8A.X を使用します(PCM8.Xではありません)。

[PCM8A.X](http://retropc.net/x68000/software/sound/adpcm/pcm8a/)

PCM8A.X が導入されていない場合、または `-a` オプションをつけた場合は MP3EXP 自身で ADPCMエンコードを行います。精度は担保されますが、処理パフォーマンスは落ちます。MP3ファイルの再生時には PCM8A.X が必ず必要になります。

`-b` オプションでリンクアレイチェーンのバッファ数を指定します。1バッファが約64KBです。デフォルトは4です。再生が追いつかずにバッファアンダーランのエラーで途中終了してしまうような場合は大きくしてみてください。

`-u` オプションで可能な限り060turboのハイメモリからアロケートします。ただしDMACの転送が必要な部分についてはオプションの指定によらずメインメモリからアロケートします。MP3EXP.X は 060loadhigh によりプログラム部分のハイメモリ実行に対応していますが、060high でアロケート領域もハイメモリから取る実行はしないでください。

`-l` オプションでループ回数を指定します。数字を省略した場合は無限ループになります。

---

### データフォーマットごとの要求MPU能力

* ADPCMファイル

エミュレータでの検証しかしていませんが、ディスク読み込みが遅くなければ 68000 10MHz機でも大丈夫なはずです。

* PCMファイル

エミュレータでの検証では S44 (44.1kHz 16bit PCM stereo) の再生には最低 68000 24MHz が必要です。実機ではX68030以上を推奨します。

* MP3ファイル

エミュレータと実機での検証結果は以下になります。

機種|MP3デコーダ|ADPCMエンコーダ|バッファ|MP3(固定128k)|MP3(固定256k)|MP3(VBR約256k)
-|-|-|-|-|-|-
XEiJ X68000 Hybrid (600MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|buffer underrun|buffer underrun|buffer underrun
XEiJ X68000 Hybrid (700MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|buffer underrun|buffer underrun|buffer underrun
XEiJ X68000 Hybrid (800MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|buffer underrun|buffer underrun
XEiJ X68000 Hybrid (900MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|OK|OK
XEiJ X68000 Hybrid (999.9MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|OK|OK
-|-|-|-|-|-|-
XEiJ X68030 (400MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|buffer underrun|buffer underrun|buffer underrun
XEiJ X68030 (500MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|buffer underrun|buffer underrun|buffer underrun
XEiJ X68030 (600MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|buffer underrun|buffer underrun
XEiJ X68030 (700MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|OK|OK
XEiJ X68030 (800MHz)|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB|OK|OK|OK
-|-|-|-|-|-|-
XEiJ 060turbo (33MHz) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB (+ハイメモリ)|buffer underrun|buffer underrun|buffer underrun
XEiJ 060turbo (50MHz) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB (+ハイメモリ)|buffer underrun|buffer underrun|buffer underrun
XEiJ 060turbo (75MHz) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB (+ハイメモリ)|OK|OK|OK
XEiJ 060turbo (100MHz) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB (+ハイメモリ)|OK|OK|OK
-|-|-|-|-|-|-
実機 060turbo(50MHz,スタカラOFF) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|4 * 64KB (+ハイメモリ)			
実機 060turbo(50MHz,スタカラOFF) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|8 * 64KB (+ハイメモリ)			
実機 060turbo(50MHz,スタカラOFF) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|12 * 64KB (+ハイメモリ)			
実機 060turbo(50MHz,スタカラOFF) + 060loadhigh|MP3EXP(0.3.0)|PCM8A(1.02)|16 * 64KB (+ハイメモリ)			

---

### License

MP3デコードライブラリとして libmad 0.15.1b をx68k向けにコンパイルしたものを利用させて頂いており、ライセンスは libmad のもの (GPLv2) に準じます。

---

### Special Thanks

* xdev68k thanks to ファミべのよっしんさん
* HAS060.X on run68mac thanks to YuNKさん / M.Kamadaさん / GOROmanさん
* HLK301.X on run68mac thanks to SALTさん / GOROmanさん
* XEiJ & 060turbo.sys thanks to M.Kamadaさん

---

### History

* 0.3.0 (2023/02/16) ... 初版