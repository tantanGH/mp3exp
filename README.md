# MP3EXP.X
ADPCM/PCM/MP3 player for X680x0/Human68k

以下の形式のファイルを内蔵ADPCMを使って再生します。出力音質はすべてADPCM 15.6kHz monoとなります。
ファイルの種別は拡張子で判断します。

 - X68K(MSM6258V) ADPCM 15.6kHz mono (.PCM)
 - 16bit符号付き big endian raw PCM (.S32/.S44/.S48/.M32/.M44/.M48)
 - MP3 (.MP3)

注意：MP3ファイルの再生はかなりのMPUパワーを必要とします。事実上エミュレータ向けのオマケです。

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
         -q    ... mp3 high quality mode
         -h    ... show help message

デフォルトではADPCMのエンコードに philly さんの PCM8A.X を使用します(オリジナルのPCM8.Xではありません)。

[PCM8A.X](http://retropc.net/x68000/software/sound/adpcm/pcm8a/)

PCM8A.X が常駐していない場合、または `-a` オプションをつけた場合は MP3EXP 自身で ADPCMエンコードを行います。精度は担保されますが、処理パフォーマンスが落ちます。MP3ファイルの再生時には PCM8A.X が必ず必要になります。

`-b` オプションでリンクアレイチェーンのバッファ数を指定します。1バッファが約64KBです。デフォルトは4です。再生が追いつかずにバッファアンダーランのエラーで途中終了してしまうような場合は大きくしてみてください。

`-u` オプションで可能な限り060turboのハイメモリからアロケートします。ただしDMACの転送が必要な部分についてはオプションの指定によらずメインメモリからアロケートします。MP3EXP.X は 060loadhigh によりプログラム部分のハイメモリ実行に対応していますが、060high でアロケート領域もハイメモリから取る実行はしないでください。

`-l` オプションでループ回数を指定します。数字を省略した場合は無限ループになります。

`-q` オプションでMP3再生時の品質を落とさないようにします。つけない場合は速度優先で若干品質が落ちます。つけた場合は MP3EX.X コンバータと同じ品質になります。

---

### データフォーマットごとの要求MPU能力

* ADPCMファイル

エミュレータでの検証しかしていませんが、ディスク読み込みが遅くなければ X68000 10~16MHz機でも大丈夫かもしれません。

* PCMファイル

エミュレータでの検証では S44 (44.1kHz 16bit PCM stereo) の再生には最低 X68000 24MHz が必要です。実機ではX68030以上を推奨します。

* MP3ファイル

エミュレータでの検証では X68000-600MHz, X68030-500MHz, 060turbo-50MHz が必要です。

我が家の X68030+060turbo実機(定格50MHz、非高速化版、スタカラOFF)の場合、バッファを十分に確保すれば通常の長さの曲をどうにか再生しきることができました。

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

* 0.4.0 (2023/02/17) ... 初版