# コールバック再入防止機能

## 概要

`Params` クラスのコールバック実行中に、インスタンスの状態を変更する危険なメソッドが呼ばれた場合の未定義動作を防ぐため、再入防止機構を実装しました。

## 問題点

従来の実装では、以下のような状況で未定義動作が発生する可能性がありました:

```cpp
params.on_change([&params]() {
    // コールバック内から危険なメソッドを呼び出す
    params.set_file("other.json");     // ファイルパス変更
    params.unbind_all();                // 全バインド削除
    params.invalidate_cache();          // キャッシュクリア
    params.reset_to_defaults();         // デフォルト値リセット
    params.update();                    // 再度更新を試行（無限ループの危険）
});
```

「ロック外出し」パターン自体は正しいアプローチですが、コールバック内でインスタンスの寿命や状態を大きく変更する処理が呼ばれると、以下の問題が発生します:

1. **再帰的呼び出し**: `update()` → コールバック → `update()` → ... の無限ループ
2. **状態の不整合**: コールバック実行中にバインディングやキャッシュが削除される
3. **未定義動作**: ファイルウォッチャーの再初期化中に別の処理が走る

## 解決方法

### 1. 再入防止フラグの追加

`Params` クラスに `std::atomic<bool> in_callback_` フラグを追加:

```cpp
// 再入防止フラグ(コールバック実行中かどうか)
std::atomic<bool> in_callback_{false};
```

### 2. update() での再入チェック

コールバック実行前後でフラグを管理:

```cpp
bool update() {
    // コールバック実行中の再入を防止
    if (in_callback_.load()) {
        internal::log(LogLevel::Debug, 
            "update() called during callback execution - skipped to prevent recursion");
        return false;
    }
    
    // ... ファイル読み込み処理 ...
    
    // コールバック実行
    if (callback_to_invoke) {
        in_callback_.store(true);
        try {
            callback_to_invoke();
        } catch (...) {
            in_callback_.store(false);
            throw;
        }
        in_callback_.store(false);
    }
    
    return updated;
}
```

### 3. 危険なメソッドでの再入チェック

以下のメソッドでコールバック実行中の呼び出しをブロック:

```cpp
void set_file(std::string_view file_path, FileFormat format = FileFormat::Auto) {
    if (in_callback_.load()) {
        internal::log(LogLevel::Warning, 
            "set_file() called during callback execution - operation skipped");
        return;
    }
    // ... 通常処理 ...
}

void unbind_all() {
    if (in_callback_.load()) {
        internal::log(LogLevel::Warning, 
            "unbind_all() called during callback execution - operation skipped");
        return;
    }
    // ... 通常処理 ...
}

// 同様に以下のメソッドも保護:
// - start_watching()
// - stop_watching()
// - invalidate_cache()
// - reset_to_defaults()
```

## 動作

### 正常な使用例

```cpp
Params params("config.json");
float speed = 1.0f;
params.bind("speed", speed, 1.0f);

params.on_change([&]() {
    std::cout << "Speed changed to: " << speed << std::endl;
    // 値の読み取りやロギングは安全
});

params.update(); // OK: コールバックが安全に実行される
```

### 再入が防止される例

```cpp
params.on_change([&params]() {
    // これらの呼び出しは警告ログを出力して無視される
    params.update();              // デバッグログ: 再帰防止のためスキップ
    params.set_file("other.json"); // 警告ログ: 処理をスキップ
    params.unbind_all();           // 警告ログ: 処理をスキップ
});
```

## テスト

`Test/test_reentrancy.cpp` に以下のテストケースを実装:

1. ✅ `update()` の再帰呼び出し防止
2. ✅ `set_file()` の再入防止
3. ✅ `unbind_all()` の再入防止
4. ✅ `invalidate_cache()` の再入防止
5. ✅ `reset_to_defaults()` の再入防止

全てのテストが成功することを確認済み。

## パフォーマンス影響

- `std::atomic<bool>` の読み書きは非常に軽量（数ナノ秒）
- コールバックが無い場合: オーバーヘッドはほぼゼロ
- コールバックがある場合: フラグのセット/リセットのみ（例外安全）

## 互換性

この変更は既存のコードとの互換性を完全に保ちます:

- ✅ API変更なし
- ✅ 正常な使用方法に影響なし
- ✅ 危険な使い方のみを安全にブロック

## まとめ

**修正前の問題**: コールバック内で `reset_to_defaults()`、`set_file()` などを呼ぶと未定義動作が発生する可能性

**修正後の動作**: これらの呼び出しは安全にブロックされ、警告ログが出力される

これにより、ユーザーが意図せず危険な操作を行った場合でも、プログラムがクラッシュすることなく、デバッグ可能な状態を維持できます。
