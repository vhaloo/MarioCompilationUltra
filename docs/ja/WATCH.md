# Mario Compilation Ultra 0.8.0 · 時計の表示

[English](../en/WATCH.md) · [Français](../fr/WATCH.md) · [Español](../es/WATCH.md) · [日本語](../ja/WATCH.md) · [Deutsch](../de/WATCH.md) · [Italiano](../it/WATCH.md) · [Português](../pt/WATCH.md) · [Русский](../ru/WATCH.md) · [简体中文](../zh/WATCH.md) · [한국어](../ko/WATCH.md)

![時計の表示](watch.svg)

リューズやベゼルを回すとズーム、メニューではスクロールします。タッチの+と-も使えます。

0%は読み込み済みエリア全体、100%は元のカメラです。扉や土管で別エリアを読み込む場合があります。

多くのゲームでBはジャンプ、Yは走る・つかむ操作です。方向とアクションを別の指で同時に押せます。

一時停止アイコンかAndroidの戻るで停止します。再開で同じゲームを続けます。終了はコレクションの一番下です。

個人用パッケージには所有するローカルカートリッジが含まれます。公開版では自分の対応ファイルを取り込んでください。

CartridgeInboxに転送し、転送したカートリッジを確認するかアプリを再起動してください。既存ファイルは保護されます。

実機時計の操作感と長時間性能は未検証です。このエディションは実験版です。

実機時計の操作感と長時間性能は未検証です。このエディションは実験版です。

署名済みAPKの更新はアプリデータを保ちます。アンインストールやデータ消去ではローカルのカートリッジとセーブが消えます。

```text
/sdcard/Android/data/com.vhaloo.mariocompilationultra.wear/files/CartridgeInbox/

adb install -r MarioCompilationUltra-0.8.0-Wear-public.apk
adb push cartridge.sfc /sdcard/Android/data/com.vhaloo.mariocompilationultra.wear/files/CartridgeInbox/
```

[Wear APK](https://github.com/vhaloo/MarioCompilationUltra/releases/download/v0.8.0/MarioCompilationUltra-0.8.0-Wear-public.apk) · [HTML](watch.html) · [図解設定ガイド](SETTINGS.md) · [Guide](README.md)
