# KIZAMI

ドラムループ用のステップシーケンサー・プラグイン(VST3 / AU / 単体アプリ、JUCE 8製)。

- 音を好きなだけ追加(キック、808、スネア、クラップ、ハットなど12種類の内蔵音 + 自分のサンプル)
- 1〜16小節、スウィング、強弱、ハットロール
- ドラムパッド・キーボード・MIDIで叩いて録音
- プリセット保存(サンプルも埋め込み)
- WAV / MIDI をDAWへドラッグ、トラック別WAVの書き出し
- DAWのテンポと再生位置に同期

## Mac用ビルド

GitHub Actions(`.github/workflows/build-mac.yml`)が、pushするたびに Universal(Apple Silicon + Intel)の
VST3 / AU / 単体アプリをビルドし、`Kizami-mac.zip` として Artifacts に置きます。

手元のMacでビルドする場合:

```bash
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release -j 8
```

インストールと使い方は `dist-extras/はじめにお読みください.txt` を参照。

## テスト

`test/` に、DAWと同じようにプラグインを動かして発音タイミング・録音・保存・書き出しを確認するテストがあります。
