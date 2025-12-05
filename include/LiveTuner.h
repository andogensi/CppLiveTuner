#pragma once

/**
 * @file LiveTuner.h
 * @brief Live Parameter Tuning Library (STB-style single-header)
 * 
 * This is an STB-style header-only library. To use:
 * 
 * 1. In ONE source file, define LIVETUNER_IMPLEMENTATION before including:
 *    @code
 *    #define LIVETUNER_IMPLEMENTATION
 *    #include "LiveTuner.h"
 *    @endcode
 * 
 * 2. In all other files, just include normally:
 *    @code
 *    #include "LiveTuner.h"
 *    @endcode
 * 
 * This design avoids polluting your project with Windows.h and other
 * platform-specific headers in most translation units.
 * 
 * This library includes picojson by Kazuho Oku and Cybozu Labs, Inc.
 * picojson is licensed under the 2-clause BSD License.
 * See the picojson license section at the end of this file.
 * 
 * To use an external picojson instead of the embedded version,
 * define LIVETUNER_USE_EXTERNAL_PICOJSON before including this header:
 *   #define LIVETUNER_USE_EXTERNAL_PICOJSON
 *   #include "picojson.h"
 *   #include "LiveTuner.h"
 * 
 * Allows injecting values into a running program by simply modifying
 * a text file without recompilation.
 * 
 * Features:
 * - Event-driven file watching using OS native APIs (Windows/Linux/macOS)
 * - Non-blocking API (suitable for game loops)
 * - Named parameters (simultaneous monitoring of multiple values)
 * - Support for JSON/YAML/INI formats
 * - STB-style header-only, C++17 or later
 * 
 * Example (single value):
 * @code
 * float speed = 1.0f;
 * while (running) {
 *     tune_try(speed);  // Monitor params.txt for changes and update value
 *     player.move(speed);
 * }
 * @endcode
 * 
 * Example (named parameters):
 * @code
 * livetuner::Params params("config.json");
 * float speed = 1.0f, gravity = 9.8f;
 * bool debug = false;
 * 
 * params.bind("speed", speed, 1.0f);      // With default value
 * params.bind("gravity", gravity, 9.8f);
 * params.bind("debug", debug, false);
 * 
 * while (running) {
 *     params.update();  // Update all if changed
 *     // speed, gravity, debug are now at latest values
 * }
 * @endcode
 */

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <thread>
#include <sstream>
#include <memory>
#include <future>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <vector>
#include <unordered_map>
#include <map>
#include <variant>
#include <optional>
#include <any>
#include <typeindex>
#include <cmath>
#include <algorithm>
#include <cctype>

// ============================================================
// Configuration Macros
// ============================================================

/**
 * @def LIVETUNER_ENABLE_DEFAULT_LOGGING
 * @brief Enable/disable default logging to stderr
 * 
 * By default, LiveTuner outputs log messages to std::cerr if no custom
 * log callback is set. This is useful for development and debugging.
 * 
 * However, in GUI-only applications (especially on Windows) or in
 * production environments where console output is not desired,
 * you can disable this default logging by defining:
 * 
 *   #define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
 * 
 * before including this header. When disabled, logs will only be output
 * through a custom log callback set via set_log_callback().
 * 
 * Default: 1 (enabled) for debug builds, 0 (disabled) for release builds
 */
#ifndef LIVETUNER_ENABLE_DEFAULT_LOGGING
  #if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
    #define LIVETUNER_ENABLE_DEFAULT_LOGGING 1
  #else
    #define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
  #endif
#endif

// ============================================================
// Platform-specific headers
// ============================================================
// 
// Note: Platform-specific headers (Windows.h, inotify, FSEvents) are
// only included in the implementation section (LIVETUNER_IMPLEMENTATION).
// This keeps your compilation clean from Windows.h pollution in most
// translation units.
//
// The FileWatcher class uses PIMPL idiom to hide platform details.

namespace livetuner {

// ============================================================
// Error Handling
// ============================================================

/**
 * @brief Error types
 */
enum class ErrorType {
    None,                   ///< No error
    FileNotFound,          ///< File does not exist
    FileAccessDenied,      ///< File access denied
    FileEmpty,             ///< File is empty
    FileReadError,         ///< Failed to read file
    ParseError,            ///< Parse error (syntax error)
    InvalidFormat,         ///< Invalid format
    Timeout,               ///< Timeout
    WatcherError,          ///< Failed to start file watcher
    Unknown                ///< Unknown error
};

/**
 * @brief Error information
 */
struct ErrorInfo {
    ErrorType type = ErrorType::None;
    std::string message;
    std::string file_path;
    std::chrono::system_clock::time_point timestamp;
    
    ErrorInfo() = default;
    
    ErrorInfo(ErrorType t, std::string msg, std::string path = "")
        : type(t)
        , message(std::move(msg))
        , file_path(std::move(path))
        , timestamp(std::chrono::system_clock::now())
    {}
    
    /**
     * @brief Check if there is an error
     */
    explicit operator bool() const {
        return type != ErrorType::None;
    }
    
    /**
     * @brief Convert error type to string
     */
    static const char* type_to_string(ErrorType type) {
        switch (type) {
            case ErrorType::None: return "None";
            case ErrorType::FileNotFound: return "FileNotFound";
            case ErrorType::FileAccessDenied: return "FileAccessDenied";
            case ErrorType::FileEmpty: return "FileEmpty";
            case ErrorType::FileReadError: return "FileReadError";
            case ErrorType::ParseError: return "ParseError";
            case ErrorType::InvalidFormat: return "InvalidFormat";
            case ErrorType::Timeout: return "Timeout";
            case ErrorType::WatcherError: return "WatcherError";
            case ErrorType::Unknown: return "Unknown";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Convert error information to string
     */
    std::string to_string() const {
        if (type == ErrorType::None) {
            return "No error";
        }
        
        std::ostringstream oss;
        oss << "[" << type_to_string(type) << "] ";
        if (!file_path.empty()) {
            oss << file_path << ": ";
        }
        oss << message;
        return oss.str();
    }
};

/**
 * @brief Log level
 */
enum class LogLevel {
    Debug,      ///< Debug information
    Info,       ///< General information
    Warning,    ///< Warning
    Error       ///< Error
};

/**
 * @brief Log callback type
 */
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

namespace internal {
    /**
     * @brief Default log handler (outputs to stderr)
     */
    inline void default_log_handler(LogLevel level, const std::string& message) {
#if LIVETUNER_ENABLE_DEFAULT_LOGGING
        const char* level_str = "INFO";
        switch (level) {
            case LogLevel::Debug:   level_str = "DEBUG"; break;
            case LogLevel::Info:    level_str = "INFO"; break;
            case LogLevel::Warning: level_str = "WARN"; break;
            case LogLevel::Error:   level_str = "ERROR"; break;
        }
        std::cerr << "[LiveTuner:" << level_str << "] " << message << std::endl;
#else
        // No default logging when disabled
        (void)level;
        (void)message;
#endif
    }
} // namespace internal

/**
 * @brief Global log callback (optional)
 */
inline LogCallback& get_global_log_callback() {
    static LogCallback callback = internal::default_log_handler;
    return callback;
}

/**
 * @brief Set global log callback
 * 
 * By default, LiveTuner uses an internal log handler that outputs to std::cerr
 * (only in debug builds unless LIVETUNER_ENABLE_DEFAULT_LOGGING is explicitly set).
 * 
 * You can replace this with your own logging system:
 * 
 * Example (custom logger):
 * @code
 * livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
 *     MyLogger::log(level, msg);  // Use your own logging system
 * });
 * @endcode
 * 
 * Example (disable all logging):
 * @code
 * livetuner::set_log_callback(nullptr);
 * @endcode
 * 
 * Example (standard cerr output):
 * @code
 * livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
 *     const char* level_str = "INFO";
 *     switch (level) {
 *         case livetuner::LogLevel::Debug: level_str = "DEBUG"; break;
 *         case livetuner::LogLevel::Info: level_str = "INFO"; break;
 *         case livetuner::LogLevel::Warning: level_str = "WARN"; break;
 *         case livetuner::LogLevel::Error: level_str = "ERROR"; break;
 *     }
 *     std::cerr << "[LiveTuner:" << level_str << "] " << msg << std::endl;
 * });
 * @endcode
 * 
 * @param callback The log callback function. Pass nullptr to disable logging.
 */
inline void set_log_callback(LogCallback callback) {
    get_global_log_callback() = std::move(callback);
}

// ============================================================
// Vendored: picojson
// ============================================================
// picojson - a C++ JSON parser / serializer
// Copyright 2009-2010 Cybozu Labs, Inc.
// Copyright 2011-2014 Kazuho Oku
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ============================================================
//
// Note: If you want to use an external picojson instead of the embedded version,
// define LIVETUNER_USE_EXTERNAL_PICOJSON before including this header.
// Example:
//   #define LIVETUNER_USE_EXTERNAL_PICOJSON
//   #include "picojson.h"  // Your external picojson
//   #include "LiveTuner.h"

#ifndef LIVETUNER_USE_EXTERNAL_PICOJSON

namespace picojson {

enum {
  null_type,
  boolean_type,
  number_type,
  string_type,
  array_type,
  object_type
#ifdef PICOJSON_USE_INT64
  ,
  int64_type
#endif
};

enum { INDENT_WIDTH = 2, DEFAULT_MAX_DEPTHS = 100 };

struct null {};

class value {
public:
  typedef std::vector<value> array;
  typedef std::map<std::string, value> object;
  union _storage {
    bool boolean_;
    double number_;
#ifdef PICOJSON_USE_INT64
    int64_t int64_;
#endif
    std::string *string_;
    array *array_;
    object *object_;
  };

protected:
  int type_;
  _storage u_;

public:
  value();
  value(int type, bool);
  explicit value(bool b);
#ifdef PICOJSON_USE_INT64
  explicit value(int64_t i);
#endif
  explicit value(double n);
  explicit value(const std::string &s);
  explicit value(const array &a);
  explicit value(const object &o);
  explicit value(const char *s);
  value(const char *s, size_t len);
  ~value();
  value(const value &x);
  value &operator=(const value &x);
  void swap(value &x) noexcept;
  template <typename T> bool is() const;
  template <typename T> const T &get() const;
  template <typename T> T &get();
  template <typename T> void set(const T &);
  bool evaluate_as_boolean() const;
  const value &get(const size_t idx) const;
  const value &get(const std::string &key) const;
  value &get(const size_t idx);
  value &get(const std::string &key);

  bool contains(const size_t idx) const;
  bool contains(const std::string &key) const;
  std::string to_str() const;
  template <typename Iter> void serialize(Iter os, bool prettify = false) const;
  std::string serialize(bool prettify = false) const;

private:
  template <typename T> value(const T *);
  template <typename Iter> static void _indent(Iter os, int indent);
  template <typename Iter> void _serialize(Iter os, int indent) const;
  std::string _serialize(int indent) const;
  void clear();
};

typedef value::array array;
typedef value::object object;

inline value::value() : type_(null_type), u_() {
}

inline value::value(int type, bool) : type_(type), u_() {
  switch (type) {
#define INIT(p, v)                                                                                                                 \
  case p##type:                                                                                                                    \
    u_.p = v;                                                                                                                      \
    break
    INIT(boolean_, false);
    INIT(number_, 0.0);
#ifdef PICOJSON_USE_INT64
    INIT(int64_, 0);
#endif
    INIT(string_, new std::string());
    INIT(array_, new array());
    INIT(object_, new object());
#undef INIT
  default:
    break;
  }
}

inline value::value(bool b) : type_(boolean_type), u_() {
  u_.boolean_ = b;
}

#ifdef PICOJSON_USE_INT64
inline value::value(int64_t i) : type_(int64_type), u_() {
  u_.int64_ = i;
}
#endif

inline value::value(double n) : type_(number_type), u_() {
  if (std::isnan(n) || std::isinf(n)) {
    throw std::overflow_error("");
  }
  u_.number_ = n;
}

inline value::value(const std::string &s) : type_(string_type), u_() {
  u_.string_ = new std::string(s);
}

inline value::value(const array &a) : type_(array_type), u_() {
  u_.array_ = new array(a);
}

inline value::value(const object &o) : type_(object_type), u_() {
  u_.object_ = new object(o);
}

inline value::value(const char *s) : type_(string_type), u_() {
  u_.string_ = new std::string(s);
}

inline value::value(const char *s, size_t len) : type_(string_type), u_() {
  u_.string_ = new std::string(s, len);
}

inline void value::clear() {
  switch (type_) {
#define DEINIT(p)                                                                                                                  \
  case p##type:                                                                                                                    \
    delete u_.p;                                                                                                                   \
    break
    DEINIT(string_);
    DEINIT(array_);
    DEINIT(object_);
#undef DEINIT
  default:
    break;
  }
}

inline value::~value() {
  clear();
}

inline value::value(const value &x) : type_(x.type_), u_() {
  switch (type_) {
#define INIT(p, v)                                                                                                                 \
  case p##type:                                                                                                                    \
    u_.p = v;                                                                                                                      \
    break
    INIT(string_, new std::string(*x.u_.string_));
    INIT(array_, new array(*x.u_.array_));
    INIT(object_, new object(*x.u_.object_));
#undef INIT
  default:
    u_ = x.u_;
    break;
  }
}

inline value &value::operator=(const value &x) {
  if (this != &x) {
    value t(x);
    swap(t);
  }
  return *this;
}

inline void value::swap(value &x) noexcept {
  std::swap(type_, x.type_);
  std::swap(u_, x.u_);
}

#define IS(ctype, jtype)                                                                                                           \
  template <> inline bool value::is<ctype>() const {                                                                               \
    return type_ == jtype##_type;                                                                                                  \
  }
IS(null, null)
IS(bool, boolean)
#ifdef PICOJSON_USE_INT64
IS(int64_t, int64)
#endif
IS(std::string, string)
IS(array, array)
IS(object, object)
#undef IS
template <> inline bool value::is<double>() const {
  return type_ == number_type
#ifdef PICOJSON_USE_INT64
         || type_ == int64_type
#endif
      ;
}

#define GET(ctype, var)                                                                                                            \
  template <> inline const ctype &value::get<ctype>() const {                                                                      \
    if (!is<ctype>()) throw std::runtime_error("type mismatch! call is<type>() before get<type>()");                              \
    return var;                                                                                                                    \
  }                                                                                                                                \
  template <> inline ctype &value::get<ctype>() {                                                                                  \
    if (!is<ctype>()) throw std::runtime_error("type mismatch! call is<type>() before get<type>()");                              \
    return var;                                                                                                                    \
  }
GET(bool, u_.boolean_)
GET(std::string, *u_.string_)
GET(array, *u_.array_)
GET(object, *u_.object_)
#ifdef PICOJSON_USE_INT64
GET(double,
    (type_ == int64_type && (const_cast<value *>(this)->type_ = number_type, (const_cast<value *>(this)->u_.number_ = u_.int64_)),
     u_.number_))
GET(int64_t, u_.int64_)
#else
GET(double, u_.number_)
#endif
#undef GET

#define SET(ctype, jtype, setter)                                                                                                  \
  template <> inline void value::set<ctype>(const ctype &_val) {                                                                   \
    clear();                                                                                                                       \
    type_ = jtype##_type;                                                                                                          \
    setter                                                                                                                         \
  }
SET(bool, boolean, u_.boolean_ = _val;)
SET(std::string, string, u_.string_ = new std::string(_val);)
SET(array, array, u_.array_ = new array(_val);)
SET(object, object, u_.object_ = new object(_val);)
SET(double, number, u_.number_ = _val;)
#ifdef PICOJSON_USE_INT64
SET(int64_t, int64, u_.int64_ = _val;)
#endif
#undef SET

inline bool value::evaluate_as_boolean() const {
  switch (type_) {
  case null_type:
    return false;
  case boolean_type:
    return u_.boolean_;
  case number_type:
    return u_.number_ != 0;
#ifdef PICOJSON_USE_INT64
  case int64_type:
    return u_.int64_ != 0;
#endif
  case string_type:
    return !u_.string_->empty();
  default:
    return true;
  }
}

inline const value &value::get(const size_t idx) const {
  static value s_null;
  if (!is<array>()) throw std::runtime_error("type mismatch");
  return idx < u_.array_->size() ? (*u_.array_)[idx] : s_null;
}

inline value &value::get(const size_t idx) {
  static value s_null;
  if (!is<array>()) throw std::runtime_error("type mismatch");
  return idx < u_.array_->size() ? (*u_.array_)[idx] : s_null;
}

inline const value &value::get(const std::string &key) const {
  static value s_null;
  if (!is<object>()) throw std::runtime_error("type mismatch");
  object::const_iterator i = u_.object_->find(key);
  return i != u_.object_->end() ? i->second : s_null;
}

inline value &value::get(const std::string &key) {
  static value s_null;
  if (!is<object>()) throw std::runtime_error("type mismatch");
  object::iterator i = u_.object_->find(key);
  return i != u_.object_->end() ? i->second : s_null;
}

inline bool value::contains(const size_t idx) const {
  if (!is<array>()) throw std::runtime_error("type mismatch");
  return idx < u_.array_->size();
}

inline bool value::contains(const std::string &key) const {
  if (!is<object>()) throw std::runtime_error("type mismatch");
  object::const_iterator i = u_.object_->find(key);
  return i != u_.object_->end();
}

inline std::string value::to_str() const {
  switch (type_) {
  case null_type:
    return "null";
  case boolean_type:
    return u_.boolean_ ? "true" : "false";
#ifdef PICOJSON_USE_INT64
  case int64_type: {
    return std::to_string(u_.int64_);
  }
#endif
  case number_type: {
    return std::to_string(u_.number_);
  }
  case string_type:
    return *u_.string_;
  case array_type:
    return "array";
  case object_type:
    return "object";
  default:
    return std::string();
  }
}

template <typename Iter> void copy(const std::string &s, Iter oi) {
  std::copy(s.begin(), s.end(), oi);
}

template <typename Iter> struct serialize_str_char {
  Iter oi;
  void operator()(char c) {
    switch (c) {
#define MAP(val, sym)                                                                                                              \
  case val:                                                                                                                        \
    copy(sym, oi);                                                                                                                 \
    break
      MAP('"', "\\\"");
      MAP('\\', "\\\\");
      MAP('/', "\\/");
      MAP('\b', "\\b");
      MAP('\f', "\\f");
      MAP('\n', "\\n");
      MAP('\r', "\\r");
      MAP('\t', "\\t");
#undef MAP
    default:
      if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
        copy(buf, buf + 6, oi);
      } else {
        *oi++ = c;
      }
      break;
    }
  }
};

template <typename Iter> void serialize_str(const std::string &s, Iter oi) {
  *oi++ = '"';
  serialize_str_char<Iter> process_char = {oi};
  std::for_each(s.begin(), s.end(), process_char);
  *oi++ = '"';
}

template <typename Iter> void value::serialize(Iter oi, bool prettify) const {
  return _serialize(oi, prettify ? 0 : -1);
}

inline std::string value::serialize(bool prettify) const {
  return _serialize(prettify ? 0 : -1);
}

template <typename Iter> void value::_indent(Iter oi, int indent) {
  *oi++ = '\n';
  for (int i = 0; i < indent * INDENT_WIDTH; ++i) {
    *oi++ = ' ';
  }
}

template <typename Iter> void value::_serialize(Iter oi, int indent) const {
  switch (type_) {
  case string_type:
    serialize_str(*u_.string_, oi);
    break;
  case array_type: {
    *oi++ = '[';
    if (indent != -1) {
      ++indent;
    }
    for (array::const_iterator i = u_.array_->begin(); i != u_.array_->end(); ++i) {
      if (i != u_.array_->begin()) {
        *oi++ = ',';
      }
      if (indent != -1) {
        _indent(oi, indent);
      }
      i->_serialize(oi, indent);
    }
    if (indent != -1) {
      --indent;
      if (!u_.array_->empty()) {
        _indent(oi, indent);
      }
    }
    *oi++ = ']';
    break;
  }
  case object_type: {
    *oi++ = '{';
    if (indent != -1) {
      ++indent;
    }
    for (object::const_iterator i = u_.object_->begin(); i != u_.object_->end(); ++i) {
      if (i != u_.object_->begin()) {
        *oi++ = ',';
      }
      if (indent != -1) {
        _indent(oi, indent);
      }
      serialize_str(i->first, oi);
      *oi++ = ':';
      if (indent != -1) {
        *oi++ = ' ';
      }
      i->second._serialize(oi, indent);
    }
    if (indent != -1) {
      --indent;
      if (!u_.object_->empty()) {
        _indent(oi, indent);
      }
    }
    *oi++ = '}';
    break;
  }
  default:
    copy(to_str(), oi);
    break;
  }
  if (indent == 0) {
    *oi++ = '\n';
  }
}

inline std::string value::_serialize(int indent) const {
  std::string s;
  _serialize(std::back_inserter(s), indent);
  return s;
}

template <typename Iter> class input {
protected:
  Iter cur_, end_;
  bool consumed_;
  int line_;

public:
  input(const Iter &first, const Iter &last) : cur_(first), end_(last), consumed_(false), line_(1) {
  }
  int getc() {
    if (consumed_) {
      if (*cur_ == '\n') {
        ++line_;
      }
      ++cur_;
    }
    if (cur_ == end_) {
      consumed_ = false;
      return -1;
    }
    consumed_ = true;
    return *cur_ & 0xff;
  }
  void ungetc() {
    consumed_ = false;
  }
  Iter cur() const {
    if (consumed_) {
      input<Iter> *self = const_cast<input<Iter> *>(this);
      self->consumed_ = false;
      ++self->cur_;
    }
    return cur_;
  }
  int line() const {
    return line_;
  }
  void skip_ws() {
    while (1) {
      int ch = getc();
      if (!(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
        ungetc();
        break;
      }
    }
  }
  bool expect(const int expected) {
    skip_ws();
    if (getc() != expected) {
      ungetc();
      return false;
    }
    return true;
  }
  bool match(const std::string &pattern) {
    for (std::string::const_iterator pi(pattern.begin()); pi != pattern.end(); ++pi) {
      if (getc() != *pi) {
        ungetc();
        return false;
      }
    }
    return true;
  }
};

template <typename Iter> inline int _parse_quadhex(input<Iter> &in) {
  int uni_ch = 0, hex;
  for (int i = 0; i < 4; i++) {
    if ((hex = in.getc()) == -1) {
      return -1;
    }
    if ('0' <= hex && hex <= '9') {
      hex -= '0';
    } else if ('A' <= hex && hex <= 'F') {
      hex -= 'A' - 0xa;
    } else if ('a' <= hex && hex <= 'f') {
      hex -= 'a' - 0xa;
    } else {
      in.ungetc();
      return -1;
    }
    uni_ch = uni_ch * 16 + hex;
  }
  return uni_ch;
}

template <typename String, typename Iter> inline bool _parse_codepoint(String &out, input<Iter> &in) {
  int uni_ch;
  if ((uni_ch = _parse_quadhex(in)) == -1) {
    return false;
  }
  if (0xd800 <= uni_ch && uni_ch <= 0xdfff) {
    if (0xdc00 <= uni_ch) {
      return false;
    }
    if (in.getc() != '\\' || in.getc() != 'u') {
      in.ungetc();
      return false;
    }
    int second = _parse_quadhex(in);
    if (!(0xdc00 <= second && second <= 0xdfff)) {
      return false;
    }
    uni_ch = ((uni_ch - 0xd800) << 10) | ((second - 0xdc00) & 0x3ff);
    uni_ch += 0x10000;
  }
  if (uni_ch < 0x80) {
    out.push_back(static_cast<char>(uni_ch));
  } else {
    if (uni_ch < 0x800) {
      out.push_back(static_cast<char>(0xc0 | (uni_ch >> 6)));
    } else {
      if (uni_ch < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (uni_ch >> 12)));
      } else {
        out.push_back(static_cast<char>(0xf0 | (uni_ch >> 18)));
        out.push_back(static_cast<char>(0x80 | ((uni_ch >> 12) & 0x3f)));
      }
      out.push_back(static_cast<char>(0x80 | ((uni_ch >> 6) & 0x3f)));
    }
    out.push_back(static_cast<char>(0x80 | (uni_ch & 0x3f)));
  }
  return true;
}

template <typename String, typename Iter> inline bool _parse_string(String &out, input<Iter> &in) {
  while (1) {
    int ch = in.getc();
    if (ch < ' ') {
      in.ungetc();
      return false;
    } else if (ch == '"') {
      return true;
    } else if (ch == '\\') {
      if ((ch = in.getc()) == -1) {
        return false;
      }
      switch (ch) {
#define MAP(sym, val)                                                                                                              \
  case sym:                                                                                                                        \
    out.push_back(val);                                                                                                            \
    break
        MAP('"', '\"');
        MAP('\\', '\\');
        MAP('/', '/');
        MAP('b', '\b');
        MAP('f', '\f');
        MAP('n', '\n');
        MAP('r', '\r');
        MAP('t', '\t');
#undef MAP
      case 'u':
        if (!_parse_codepoint(out, in)) {
          return false;
        }
        break;
      default:
        return false;
      }
    } else {
      out.push_back(static_cast<char>(ch));
    }
  }
  return false;
}

template <typename Context, typename Iter> inline bool _parse_array(Context &ctx, input<Iter> &in) {
  if (!ctx.parse_array_start()) {
    return false;
  }
  size_t idx = 0;
  if (in.expect(']')) {
    return ctx.parse_array_stop(idx);
  }
  do {
    if (!ctx.parse_array_item(in, idx)) {
      return false;
    }
    idx++;
  } while (in.expect(','));
  return in.expect(']') && ctx.parse_array_stop(idx);
}

template <typename Context, typename Iter> inline bool _parse_object(Context &ctx, input<Iter> &in) {
  if (!ctx.parse_object_start()) {
    return false;
  }
  if (in.expect('}')) {
    return ctx.parse_object_stop();
  }
  do {
    std::string key;
    if (!in.expect('"') || !_parse_string(key, in) || !in.expect(':')) {
      return false;
    }
    if (!ctx.parse_object_item(in, key)) {
      return false;
    }
  } while (in.expect(','));
  return in.expect('}') && ctx.parse_object_stop();
}

template <typename Iter> inline std::string _parse_number(input<Iter> &in) {
  std::string num_str;
  while (1) {
    int ch = in.getc();
    if (('0' <= ch && ch <= '9') || ch == '+' || ch == '-' || ch == 'e' || ch == 'E' || ch == '.') {
      num_str.push_back(static_cast<char>(ch));
    } else {
      in.ungetc();
      break;
    }
  }
  return num_str;
}

template <typename Context, typename Iter> inline bool _parse(Context &ctx, input<Iter> &in) {
  in.skip_ws();
  int ch = in.getc();
  switch (ch) {
#define IS(ch, text, op)                                                                                                           \
  case ch:                                                                                                                         \
    if (in.match(text) && op) {                                                                                                    \
      return true;                                                                                                                 \
    } else {                                                                                                                       \
      return false;                                                                                                                \
    }
    IS('n', "ull", ctx.set_null());
    IS('f', "alse", ctx.set_bool(false));
    IS('t', "rue", ctx.set_bool(true));
#undef IS
  case '"':
    return ctx.parse_string(in);
  case '[':
    return _parse_array(ctx, in);
  case '{':
    return _parse_object(ctx, in);
  default:
    if (('0' <= ch && ch <= '9') || ch == '-') {
      double f;
      char *endp;
      in.ungetc();
      std::string num_str(_parse_number(in));
      if (num_str.empty()) {
        return false;
      }
#ifdef PICOJSON_USE_INT64
      {
        errno = 0;
        intmax_t ival = strtoimax(num_str.c_str(), &endp, 10);
        if (errno == 0 && std::numeric_limits<int64_t>::min() <= ival && ival <= std::numeric_limits<int64_t>::max() &&
            endp == num_str.c_str() + num_str.size()) {
          ctx.set_int64(ival);
          return true;
        }
      }
#endif
      f = strtod(num_str.c_str(), &endp);
      if (endp == num_str.c_str() + num_str.size()) {
        ctx.set_number(f);
        return true;
      }
      return false;
    }
    break;
  }
  in.ungetc();
  return false;
}

class default_parse_context {
protected:
  value *out_;
  size_t depths_;

public:
  default_parse_context(value *out, size_t depths = DEFAULT_MAX_DEPTHS) : out_(out), depths_(depths) {
  }
  bool set_null() {
    *out_ = value();
    return true;
  }
  bool set_bool(bool b) {
    *out_ = value(b);
    return true;
  }
#ifdef PICOJSON_USE_INT64
  bool set_int64(int64_t i) {
    *out_ = value(i);
    return true;
  }
#endif
  bool set_number(double f) {
    *out_ = value(f);
    return true;
  }
  template <typename Iter> bool parse_string(input<Iter> &in) {
    *out_ = value(string_type, false);
    return _parse_string(out_->get<std::string>(), in);
  }
  bool parse_array_start() {
    if (depths_ == 0)
      return false;
    --depths_;
    *out_ = value(array_type, false);
    return true;
  }
  template <typename Iter> bool parse_array_item(input<Iter> &in, size_t) {
    array &a = out_->get<array>();
    a.push_back(value());
    default_parse_context ctx(&a.back(), depths_);
    return _parse(ctx, in);
  }
  bool parse_array_stop(size_t) {
    ++depths_;
    return true;
  }
  bool parse_object_start() {
    if (depths_ == 0)
      return false;
    *out_ = value(object_type, false);
    return true;
  }
  template <typename Iter> bool parse_object_item(input<Iter> &in, const std::string &key) {
    object &o = out_->get<object>();
    default_parse_context ctx(&o[key], depths_);
    return _parse(ctx, in);
  }
  bool parse_object_stop() {
    ++depths_;
    return true;
  }

private:
  default_parse_context(const default_parse_context &);
  default_parse_context &operator=(const default_parse_context &);
};

template <typename Context, typename Iter> inline Iter _parse(Context &ctx, const Iter &first, const Iter &last, std::string *err) {
  input<Iter> in(first, last);
  if (!_parse(ctx, in) && err != NULL) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "syntax error at line %d near: ", in.line());
    *err = buf;
    while (1) {
      int ch = in.getc();
      if (ch == -1 || ch == '\n') {
        break;
      } else if (ch >= ' ') {
        err->push_back(static_cast<char>(ch));
      }
    }
  }
  return in.cur();
}

template <typename Iter> inline Iter parse(value &out, const Iter &first, const Iter &last, std::string *err) {
  default_parse_context ctx(&out);
  return _parse(ctx, first, last, err);
}

inline std::string parse(value &out, const std::string &s) {
  std::string err;
  parse(out, s.begin(), s.end(), &err);
  return err;
}

inline std::string parse(value &out, std::istream &is) {
  std::string err;
  parse(out, std::istreambuf_iterator<char>(is.rdbuf()), std::istreambuf_iterator<char>(), &err);
  return err;
}

inline bool operator==(const value &x, const value &y) {
  if (x.is<null>())
    return y.is<null>();
#define PICOJSON_CMP(type)                                                                                                         \
  if (x.is<type>())                                                                                                                \
  return y.is<type>() && x.get<type>() == y.get<type>()
  PICOJSON_CMP(bool);
  PICOJSON_CMP(double);
  PICOJSON_CMP(std::string);
  PICOJSON_CMP(array);
  PICOJSON_CMP(object);
#undef PICOJSON_CMP
  return false;
}

inline bool operator!=(const value &x, const value &y) {
  return !(x == y);
}

} // namespace picojson

#endif // LIVETUNER_USE_EXTERNAL_PICOJSON

// End of vendored picojson
// ============================================================

namespace internal {

/**
 * @brief Log output helper
 */
inline void log(LogLevel level, const std::string& message) {
    auto& callback = get_global_log_callback();
    if (callback) {
        callback(level, message);
    }
}

// ============================================================
// RAII Resource Wrappers (Implementation in LIVETUNER_IMPLEMENTATION)
// ============================================================
// Platform-specific RAII wrappers are defined in the implementation section
// to avoid including Windows.h/inotify headers in user code.

inline auto get_file_modify_time(const std::filesystem::path& path) 
    -> std::filesystem::file_time_type {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return std::filesystem::file_time_type::min();
    }
    return ftime;
}
struct FileReadRetryConfig {
    /// Number of retries (0 to disable)
    int max_retries = 3;
    
    /// Retry interval (milliseconds)
    std::chrono::milliseconds retry_delay{5};
    
    /// Retry interval increase factor (1.0 for constant interval)
    double backoff_multiplier = 1.5;
};

/**
 * @brief File read result
 */
struct FileReadResult {
    std::optional<std::string> content;
    ErrorInfo error;
    
    explicit operator bool() const {
        return content.has_value();
    }
};

/**
 * @brief Read file contents with retry logic
 * 
 * If the editor is writing to the file, reading may fail or
 * retrieve incomplete data. This function performs retries
 * according to configuration to provide stable file reading.
 * 
 * @param path File path
 * @param config Retry configuration
 * @param error_out Error information output destination (optional)
 * @return Read file contents (empty optional on failure)
 */
inline std::optional<std::string> read_file_with_retry(
    const std::filesystem::path& path,
    const FileReadRetryConfig& config = FileReadRetryConfig{},
    ErrorInfo* error_out = nullptr) 
{
    auto delay = config.retry_delay;
    ErrorInfo last_error;
    std::string path_str = path.string();
    
    for (int attempt = 0; attempt <= config.max_retries; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(delay);
            delay = std::chrono::milliseconds(
                static_cast<int>(delay.count() * config.backoff_multiplier));
        }
        
        std::error_code ec;
        
        // Check if file exists
        if (!std::filesystem::exists(path, ec)) {
            last_error = ErrorInfo(ErrorType::FileNotFound, 
                                  "File does not exist", path_str);
            if (attempt == 0) {
                log(LogLevel::Warning, last_error.to_string());
            }
            continue;
        }
        if (ec) {
            last_error = ErrorInfo(ErrorType::FileAccessDenied,
                                  "Cannot access file: " + ec.message(), path_str);
            if (attempt == 0) {
                log(LogLevel::Warning, last_error.to_string());
            }
            continue;
        }
        
        // Get file size (skip if 0 bytes)
        auto file_size = std::filesystem::file_size(path, ec);
        if (ec) {
            last_error = ErrorInfo(ErrorType::FileReadError,
                                  "Cannot get file size: " + ec.message(), path_str);
            log(LogLevel::Debug, last_error.to_string());
            continue;
        }
        if (file_size == 0) {
            last_error = ErrorInfo(ErrorType::FileEmpty,
                                  "File is empty", path_str);
            log(LogLevel::Debug, last_error.to_string());
            continue;
        }
        
        // Open and read file
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            last_error = ErrorInfo(ErrorType::FileAccessDenied,
                                  "Cannot open file for reading", path_str);
            log(LogLevel::Debug, last_error.to_string());
            continue;
        }
        
        std::string content;
        content.reserve(static_cast<size_t>(file_size));
        
        try {
            content.assign(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());
        } catch (const std::exception& e) {
            last_error = ErrorInfo(ErrorType::FileReadError,
                                  std::string("Exception during file read: ") + e.what(), path_str);
            log(LogLevel::Debug, last_error.to_string());
            continue;
        } catch (...) {
            last_error = ErrorInfo(ErrorType::FileReadError,
                                  "Unknown exception during file read", path_str);
            log(LogLevel::Debug, last_error.to_string());
            continue;
        }
        
        // Verify successful read and expected size
        // (When writing, file size and actual read amount may differ)
        if (file.good() || file.eof()) {
            // Verify content is not empty
            if (!content.empty()) {
                if (error_out) {
                    *error_out = ErrorInfo(); // No error
                }
                return content;
            } else {
                last_error = ErrorInfo(ErrorType::FileEmpty,
                                      "File content is empty after read", path_str);
                log(LogLevel::Debug, last_error.to_string());
            }
        } else {
            last_error = ErrorInfo(ErrorType::FileReadError,
                                  "File stream in bad state after read", path_str);
            log(LogLevel::Debug, last_error.to_string());
        }
    }
    
    // All retries failed
    if (error_out) {
        *error_out = last_error;
    }
    
    if (last_error) {
        log(LogLevel::Error, "Failed to read file after " + 
            std::to_string(config.max_retries + 1) + " attempts: " + 
            last_error.to_string());
    }
    
    return std::nullopt;
}

/**
 * @brief File watcher configuration
 */
struct FileWatcherConfig {
    /// Buffer size for Windows ReadDirectoryChangesW (bytes)
    /// Default: 65536 (64KB). Increase if many file changes are expected
    /// Minimum: 4096, Maximum: 1MB (1048576)
    size_t buffer_size = 65536;
    
    /// Automatically grow buffer on overflow
    /// If true, buffer size is doubled on overflow detection (up to maximum)
    bool auto_grow_buffer = true;
    
    /// Maximum buffer size (when auto_grow_buffer is enabled)
    size_t max_buffer_size = 1048576;  // 1MB
    
    /// Callback on buffer overflow (optional)
    /// Arguments: current buffer size, new buffer size (0 if maximum reached)
    std::function<void(size_t current_size, size_t new_size)> on_buffer_overflow;
    
    /// Minimum buffer size
    static constexpr size_t min_buffer_size = 4096;
    
    /// Validate and normalize buffer size
    void validate() {
        if (buffer_size < min_buffer_size) {
            buffer_size = min_buffer_size;
        }
        if (buffer_size > max_buffer_size) {
            buffer_size = max_buffer_size;
        }
    }
};

/**
 * @brief Cross-platform file watcher class (PIMPL pattern)
 * 
 * Detects file changes event-driven using OS native APIs.
 * - Windows: ReadDirectoryChangesW
 * - Linux: inotify
 * - macOS: FSEvents
 * 
 * The implementation uses PIMPL (Pointer to Implementation) pattern to:
 * - Hide platform-specific headers (Windows.h, etc.) from user code
 * - Keep compilation clean in most translation units
 * 
 * Protection against many file changes on Windows:
 * - Expanded default buffer size to 64KB (previously 4KB)
 * - Automatic buffer expansion on overflow
 * - Overflow detection and callback notification
 * 
 * @note Requires LIVETUNER_IMPLEMENTATION to be defined in exactly ONE
 *       source file for full functionality. Without it, only polling mode
 *       is available.
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void()>;

    FileWatcher();
    explicit FileWatcher(const FileWatcherConfig& config);
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    
    FileWatcher(FileWatcher&& other) noexcept;
    FileWatcher& operator=(FileWatcher&& other) noexcept;

    /**
     * @brief Get file watcher configuration
     */
    const FileWatcherConfig& config() const { return config_; }
    
    /**
     * @brief Change file watcher configuration (effective only when not watching)
     */
    void set_config(const FileWatcherConfig& config);

    /**
     * @brief Start file watching
     * @param file_path File path to watch
     * @param callback Callback function called on file change
     * @return Whether watching started successfully
     */
    bool start(const std::filesystem::path& file_path, ChangeCallback callback);

    /**
     * @brief Stop file watching
     */
    void stop();

    /**
     * @brief Wait for file change (with timeout)
     */
    bool wait_for_change(std::chrono::milliseconds timeout);

    /**
     * @brief Wait for file change indefinitely
     */
    void wait_for_change();

    bool is_running() const;

    static bool has_native_support();

    // Accessors for native callbacks (internal use only)
    const std::filesystem::path& get_file_path() const { return file_path_; }
    void trigger_change() { notify_change(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::filesystem::path file_path_;
    ChangeCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> change_detected_{false};
    std::thread watcher_thread_;
    std::mutex cv_mtx_;
    std::condition_variable cv_;
    FileWatcherConfig config_;
    
    bool start_native();
    void stop_native();
    bool start_polling();
    void watch_polling();
    void notify_change();
};

} // namespace internal

// ============================================================
// File Format Detection and Value Parser
// ============================================================

/**
 * @brief Supported file formats
 */
enum class FileFormat {
    Auto,       ///< Auto-detect from extension
    Plain,      ///< Plain text (one value per line)
    KeyValue,   ///< key=value format (INI-style)
    Json,       ///< JSON format
    Yaml        ///< YAML format (simple support)
};

namespace internal {

/**
 * @brief Detect format from file extension
 */
inline FileFormat detect_format(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (ext == ".json") return FileFormat::Json;
    if (ext == ".yaml" || ext == ".yml") return FileFormat::Yaml;
    if (ext == ".ini" || ext == ".cfg" || ext == ".conf") return FileFormat::KeyValue;
    if (ext == ".txt") return FileFormat::Plain;
    
    // Default is KeyValue (high versatility)
    return FileFormat::KeyValue;
}

/**
 * @brief Trim string
 */
inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

/**
 * @brief Parse value from string
 */
template<typename T>
inline bool parse_value(const std::string& str, T& value) {
    std::istringstream iss(str);
    T temp;
    if (iss >> temp) {
        // Check if stream was fully consumed (no extra characters)
        std::string remaining;
        iss >> remaining;
        if (remaining.empty()) {
            value = temp;
            return true;
        }
    }
    return false;
}

// Specialization for bool type
template<>
inline bool parse_value<bool>(const std::string& str, bool& value) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        value = true;
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        value = false;
        return true;
    }
    return false;
}

// Specialization for std::string type
template<>
inline bool parse_value<std::string>(const std::string& str, std::string& value) {
    // Remove quotes
    if (str.size() >= 2 && 
        ((str.front() == '"' && str.back() == '"') ||
         (str.front() == '\'' && str.back() == '\''))) {
        value = str.substr(1, str.size() - 2);
    } else {
        value = str;
    }
    return true;
}

/**
 * @brief JSON parser using picojson
 * 
 * picojson by Kazuho Oku and Cybozu Labs, Inc. (BSD 2-Clause License)
 * Extracts values from flat JSON objects
 */
class PicojsonParser {
public:
    using ValueMap = std::unordered_map<std::string, std::string>;
    
    static bool parse(const std::string& content, ValueMap& result) {
        result.clear();
        
        picojson::value v;
        std::string err = picojson::parse(v, content);
        
        if (!err.empty() || !v.is<picojson::object>()) {
            return false;
        }
        
        const picojson::object& obj = v.get<picojson::object>();
        
        for (const auto& kv : obj) {
            const std::string& key = kv.first;
            const picojson::value& val = kv.second;
            
            if (val.is<std::string>()) {
                result[key] = val.get<std::string>();
            } else if (val.is<double>()) {
                result[key] = std::to_string(val.get<double>());
            } else if (val.is<bool>()) {
                result[key] = val.get<bool>() ? "true" : "false";
            } else if (val.is<picojson::null>()) {
                result[key] = "";
            }
            // Skip nested objects and arrays
        }
        
        return !result.empty();
    }
};

/**
 * @brief Lightweight YAML/INI parser (no external dependencies)
 * 
 * Supports key: value or key=value format
 */
class SimpleKeyValueParser {
public:
    using ValueMap = std::unordered_map<std::string, std::string>;
    
    static bool parse(const std::string& content, ValueMap& result, bool yaml_style = false) {
        result.clear();
        
        std::istringstream stream(content);
        std::string line;
        
        while (std::getline(stream, line)) {
            line = trim(line);
            
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            
            // Skip YAML document markers
            if (line == "---" || line == "...") continue;
            
            // Skip section headers (INI format)
            if (line.front() == '[' && line.back() == ']') continue;
            
            // Parse key: value or key=value
            size_t sep_pos = std::string::npos;
            
            if (yaml_style) {
                sep_pos = line.find(':');
            } else {
                // INI format: prioritize =, otherwise look for :
                sep_pos = line.find('=');
                if (sep_pos == std::string::npos) {
                    sep_pos = line.find(':');
                }
            }
            
            if (sep_pos != std::string::npos) {
                std::string key = trim(line.substr(0, sep_pos));
                std::string value = trim(line.substr(sep_pos + 1));
                
                // Remove quotes
                if (value.size() >= 2) {
                    if ((value.front() == '"' && value.back() == '"') ||
                        (value.front() == '\'' && value.back() == '\'')) {
                        value = value.substr(1, value.size() - 2);
                    }
                }
                
                if (!key.empty()) {
                    result[key] = value;
                }
            }
        }
        
        return !result.empty();
    }
};

} // namespace internal

// ============================================================
// FileWatcherConfig Export
// ============================================================

/**
 * @brief File watcher configuration (public alias)
 * 
 * Allows configuration of Windows ReadDirectoryChangesW buffer size, etc.
 * 
 * Example:
 * @code
 * livetuner::FileWatcherConfig config;
 * config.buffer_size = 262144;  // 256KB
 * config.auto_grow_buffer = true;
 * config.on_buffer_overflow = [](size_t old_size, size_t new_size) {
 *     std::cerr << "Buffer overflow detected!" << std::endl;
 * };
 * 
 * livetuner::Params params("config.json");
 * params.set_watcher_config(config);
 * params.start_watching();
 * @endcode
 */
using FileWatcherConfig = internal::FileWatcherConfig;

/**
 * @brief File read retry configuration (public alias)
 * 
 * Reading a file while an editor is writing can result in reading
 * incomplete data and causing parse errors (especially on Windows).
 * This configuration allows customization of retry behavior.
 * 
 * Example:
 * @code
 * livetuner::FileReadRetryConfig config;
 * config.max_retries = 5;          // Retry up to 5 times
 * config.retry_delay = std::chrono::milliseconds(10);  // Initial 10ms wait
 * config.backoff_multiplier = 2.0; // Double wait time each retry
 * 
 * livetuner::Params params("config.json");
 * params.set_read_retry_config(config);
 * @endcode
 */
using FileReadRetryConfig = internal::FileReadRetryConfig;

// ============================================================
// Params Class (Named Parameters)
// ============================================================

/**
 * @brief Class for managing named parameters
 * 
 * Binds multiple variables to a file and updates them in batch.
 * 
 * @note Thread Safety - Callback Execution:
 * Callbacks registered via on_change() are executed on the SAME THREAD
 * that calls update(). This is typically the main thread, making it safe
 * to call rendering APIs (OpenGL/DirectX) or other main-thread-only operations
 * from within the callback.
 * 
 * This differs from LiveTuner's event-driven mode, where callbacks may be
 * invoked from a background file-watching thread.
 * 
 * Example:
 * @code
 * // config.json:
 * // {
 * //   "speed": 1.5,
 * //   "gravity": 9.8,
 * //   "debug": true
 * // }
 * 
 * livetuner::Params params("config.json");
 * float speed, gravity;
 * bool debug;
 * 
 * params.bind("speed", speed, 1.0f);
 * params.bind("gravity", gravity, 9.8f);
 * params.bind("debug", debug, false);
 * 
 * // Callbacks run on the thread that calls update() (typically main thread),
 * // so it's safe to call rendering APIs like OpenGL/DirectX
 * params.on_change([]() {
 *     std::cout << "Config updated!\n";
 *     // Safe to update rendering settings, etc.
 * });
 * 
 * while (running) {
 *     params.update();  // Called from main thread
 *     // speed, gravity, debug are automatically updated
 * }
 * @endcode
 */
class Params {
public:
    struct FileCache {
        std::filesystem::file_time_type last_modify_time;
        std::chrono::steady_clock::time_point last_access;
        bool file_exists = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };

private:
    // Base class for holding binding information
    struct BindingBase {
        virtual ~BindingBase() = default;
        virtual bool update(const std::string& str_value) = 0;
        virtual void apply_default() = 0;
    };
    
    template<typename T>
    struct Binding : public BindingBase {
        T* target;
        T default_value;
        
        Binding(T* t, T def) : target(t), default_value(def) {}
        
        bool update(const std::string& str_value) override {
            return internal::parse_value(str_value, *target);
        }
        
        void apply_default() override {
            *target = default_value;
        }
    };

    mutable std::mutex mtx_;
    std::string file_path_;
    FileFormat format_ = FileFormat::Auto;
    FileCache file_cache_{
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    };
    
    std::unordered_map<std::string, std::unique_ptr<BindingBase>> bindings_;
    std::unordered_map<std::string, std::string> current_values_;
    
    std::unique_ptr<internal::FileWatcher> file_watcher_;
    internal::FileWatcherConfig file_watcher_config_;
    internal::FileReadRetryConfig file_read_retry_config_;
    bool use_event_driven_ = true;
    std::atomic<bool> file_changed_{false};
    
    // Error information
    ErrorInfo last_error_;
    
    // Callback
    std::function<void()> on_change_callback_;
    
    // Reentrancy prevention flag (whether callback is executing)
    std::atomic<bool> in_callback_{false};

public:
    /**
     * @brief Constructor
     * @param file_path Path to configuration file
     * @param format File format (default: auto-detect)
     */
    explicit Params(std::string_view file_path = "params.json", 
                   FileFormat format = FileFormat::Auto)
        : file_path_(file_path)
        , format_(format) 
    {
        if (format_ == FileFormat::Auto) {
            format_ = internal::detect_format(file_path_);
        }
    }
    
    /**
     * @brief Get file read retry configuration
     */
    const internal::FileReadRetryConfig& get_read_retry_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_read_retry_config_;
    }
    
    /**
     * @brief Change file read retry configuration
     * 
     * Used to avoid conflicts while editor is writing to file.
     * Especially effective on Windows.
     * 
     * Example:
     * @code
     * livetuner::Params params("config.json");
     * 
     * // Customize retry settings
     * auto config = params.get_read_retry_config();
     * config.max_retries = 5;
     * config.retry_delay = std::chrono::milliseconds(10);
     * params.set_read_retry_config(config);
     * @endcode
     */
    void set_read_retry_config(const internal::FileReadRetryConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_read_retry_config_ = config;
    }
    
    ~Params();
    
    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;
    Params(Params&&) noexcept;
    Params& operator=(Params&&) noexcept;

    /**
     * @brief Get file watcher configuration
     */
    const internal::FileWatcherConfig& get_watcher_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_watcher_config_;
    }
    
    /**
     * @brief Change file watcher configuration
     * 
     * If watching, changes take effect on next start_watching() call.
     * 
     * Example:
     * @code
     * livetuner::Params params("config.json");
     * 
     * // If many file changes are expected
     * auto config = params.get_watcher_config();
     * config.buffer_size = 262144;  // 256KB
     * config.on_buffer_overflow = [](size_t old_size, size_t new_size) {
     *     std::cerr << "Buffer overflow: " << old_size << " -> " << new_size << std::endl;
     * };
     * params.set_watcher_config(config);
     * 
     * params.start_watching();
     * @endcode
     */
    void set_watcher_config(const internal::FileWatcherConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_watcher_config_ = config;
        file_watcher_config_.validate();
    }

    /**
     * @brief Bind variable to parameter
     * 
     * @param name Parameter name (key in file)
     * @param variable Reference to variable to bind
     * @param default_value Default value if not in file
     */
    template<typename T>
    void bind(const std::string& name, T& variable, T default_value = T{}) {
        std::lock_guard<std::mutex> lock(mtx_);
        bindings_[name] = std::make_unique<Binding<T>>(&variable, default_value);
        variable = default_value;
    }

    /**
     * @brief Unbind parameter
     */
    void unbind(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        bindings_.erase(name);
    }

    /**
     * @brief Unbind all parameters
     */
    void unbind_all() {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "unbind_all() called during callback execution - operation skipped");
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        bindings_.clear();
    }

    /**
     * @brief Read file and update bound variables (non-blocking)
     * 
     * @return true if values were updated
     * 
     * @note Thread Safety:
     * This method should be called from your main thread (e.g., in the game loop).
     * Any registered callback via on_change() will be executed synchronously on
     * the SAME THREAD that calls this method, ensuring safe access to
     * main-thread-only resources like OpenGL/DirectX contexts.
     */
    bool update() {
        // Prevent reentrancy during callback execution
        if (in_callback_.load()) {
            internal::log(LogLevel::Debug, 
                "update() called during callback execution - skipped to prevent recursion");
            return false;
        }
        
        bool updated = false;
        std::function<void()> callback_to_invoke;
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            
            ensure_file_exists();
            
            auto now = std::chrono::steady_clock::now();
            auto current_modify_time = internal::get_file_modify_time(file_path_);
            
            // Cache check
            if (file_cache_.file_exists && 
                (now - file_cache_.last_access) < FileCache::cache_duration &&
                current_modify_time == file_cache_.last_modify_time) {
                return false;
            }
            
            updated = load_file();
            
            file_cache_.last_modify_time = current_modify_time;
            file_cache_.last_access = now;
            file_cache_.file_exists = true;
            
            // Copy callback and invoke outside lock (prevent deadlock)
            if (updated && on_change_callback_) {
                callback_to_invoke = on_change_callback_;
            }
        }
        
        // Invoke callback after lock release
        // Set reentrancy prevention flag and execute callback
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

    /**
     * @brief Start file watching (automatic update)
     * 
     * Monitors file in background and automatically calls update() on changes.
     */
    void start_watching() {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "start_watching() called during callback execution - operation skipped");
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (file_watcher_ && file_watcher_->is_running()) {
            return;
        }
        
        file_watcher_ = std::make_unique<internal::FileWatcher>(file_watcher_config_);
        file_changed_.store(true); // Initial read
        
        file_watcher_->start(file_path_, [this] {
            file_changed_.store(true);
        });
    }

    /**
     * @brief Stop file watching
     */
    void stop_watching() {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "stop_watching() called during callback execution - operation skipped");
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        if (file_watcher_) {
            file_watcher_->stop();
            file_watcher_.reset();
        }
    }

    /**
     * @brief Check for file changes during watching and update
     * 
     * Use in combination with start_watching().
     * 
     * @return true if values were updated
     */
    bool poll() {
        if (file_changed_.load()) {
            file_changed_.store(false);
            return update();
        }
        return false;
    }

    /**
     * @brief Set callback for changes
     * 
     * @param callback Function to be called when parameters are updated
     * 
     * @note Thread Safety - Critical for Game Development:
     * The callback is executed on the SAME THREAD that calls update(),
     * typically your main thread. This makes it SAFE to:
     * - Call OpenGL/DirectX rendering APIs
     * - Update UI elements
     * - Access main-thread-only resources
     * 
     * This is a key difference from LiveTuner's event-driven mode,
     * where callbacks may run on a background file-watching thread.
     * 
     * Example (safe OpenGL/DirectX usage):
     * @code
     * params.on_change([&shader]() {
     *     // Safe: update() is called from main thread
     *     shader.reload();  // OpenGL calls are safe here
     *     std::cout << "Shader reloaded" << std::endl;
     * });
     * 
     * while (running) {
     *     params.update();  // Callback runs here on main thread
     *     render();         // OpenGL rendering
     * }
     * @endcode
     */
    void on_change(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(mtx_);
        on_change_callback_ = std::move(callback);
    }

    /**
     * @brief Get last error information
     * 
     * @return Error information (type == ErrorType::None if no error)
     * 
     * Example:
     * @code
     * livetuner::Params params("config.json");
     * if (!params.update()) {
     *     auto error = params.last_error();
     *     if (error) {
     *         std::cerr << "Error: " << error.to_string() << std::endl;
     *     }
     * }
     * @endcode
     */
    ErrorInfo last_error() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return last_error_;
    }
    
    /**
     * @brief Check if there is an error
     */
    bool has_error() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return static_cast<bool>(last_error_);
    }
    
    /**
     * @brief Clear error
     */
    void clear_error() {
        std::lock_guard<std::mutex> lock(mtx_);
        last_error_ = ErrorInfo();
    }

    /**
     * @brief Get specific parameter value
     */
    template<typename T>
    std::optional<T> get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = current_values_.find(name);
        if (it != current_values_.end()) {
            T value;
            if (internal::parse_value(it->second, value)) {
                return value;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get specific parameter value (with default)
     */
    template<typename T>
    T get_or(const std::string& name, T default_value) const {
        auto val = get<T>(name);
        return val.value_or(default_value);
    }

    /**
     * @brief Check if parameter exists
     */
    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return current_values_.find(name) != current_values_.end();
    }

    /**
     * @brief Change file path
     */
    void set_file(std::string_view file_path, FileFormat format = FileFormat::Auto) {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "set_file() called during callback execution - operation skipped");
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        file_path_ = file_path;
        format_ = (format == FileFormat::Auto) ? internal::detect_format(file_path_) : format;
        invalidate_cache();
        
        // If watching, restart
        if (file_watcher_ && file_watcher_->is_running()) {
            file_watcher_->stop();
            file_watcher_->start(file_path_, [this] {
                file_changed_.store(true);
            });
        }
    }

    std::string get_file() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_path_;
    }

    /**
     * @brief Invalidate cache
     */
    void invalidate_cache() {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "invalidate_cache() called during callback execution - operation skipped");
            return;
        }
        
        file_cache_ = FileCache{
            std::filesystem::file_time_type::min(),
            std::chrono::steady_clock::time_point{},
            false
        };
        current_values_.clear();
    }

    /**
     * @brief Reset all binds to default values
     */
    void reset_to_defaults() {
        // Skip processing during callback execution (prevent reentrancy)
        if (in_callback_.load()) {
            internal::log(LogLevel::Warning, 
                "reset_to_defaults() called during callback execution - operation skipped");
            return;
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& [name, binding] : bindings_) {
            binding->apply_default();
        }
    }

    /**
     * @brief Get list of bound parameter names
     */
    std::vector<std::string> get_bound_names() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> names;
        names.reserve(bindings_.size());
        for (const auto& [name, _] : bindings_) {
            names.push_back(name);
        }
        return names;
    }

private:
    void ensure_file_exists() {
        if (!std::filesystem::exists(file_path_)) {
            std::ofstream file(file_path_);
            if (file) {
                switch (format_) {
                case FileFormat::Json:
                    file << "{\n";
                    file << "  // Live Tuner parameters\n";
                    file << "  // Edit values here and save\n";
                    file << "}\n";
                    break;
                case FileFormat::Yaml:
                    file << "# Live Tuner parameters\n";
                    file << "# Edit values here and save\n";
                    file << "---\n";
                    break;
                default:
                    file << "# Live Tuner parameters\n";
                    file << "# Format: key = value\n";
                    file << "\n";
                    break;
                }
            }
        }
    }

    bool load_file() {
        // Read file with retry logic
        ErrorInfo read_error;
        auto content_opt = internal::read_file_with_retry(file_path_, file_read_retry_config_, &read_error);
        if (!content_opt) {
            last_error_ = read_error;
            return false;
        }
        
        const std::string& content = *content_opt;
        
        std::unordered_map<std::string, std::string> new_values;
        bool parsed = false;
        
        switch (format_) {
        case FileFormat::Json:
            parsed = internal::PicojsonParser::parse(content, new_values);
            if (!parsed && new_values.empty()) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse JSON format", file_path_);
                internal::log(LogLevel::Error, last_error_.to_string());
            }
            break;
        case FileFormat::Yaml:
            parsed = internal::SimpleKeyValueParser::parse(content, new_values, true);
            if (!parsed && new_values.empty()) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse YAML format", file_path_);
                internal::log(LogLevel::Error, last_error_.to_string());
            }
            break;
        case FileFormat::KeyValue:
        case FileFormat::Plain:
        default:
            parsed = internal::SimpleKeyValueParser::parse(content, new_values, false);
            if (!parsed && new_values.empty()) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse key-value format", file_path_);
                internal::log(LogLevel::Error, last_error_.to_string());
            }
            break;
        }
        
        if (!parsed && new_values.empty()) {
            return false;
        }
        
        // Check if values changed
        bool any_changed = false;
        for (const auto& [key, value] : new_values) {
            auto it = current_values_.find(key);
            if (it == current_values_.end() || it->second != value) {
                any_changed = true;
                break;
            }
        }
        
        if (!any_changed && new_values.size() == current_values_.size()) {
            return false;
        }
        
        current_values_ = std::move(new_values);
        
        // Update bound variables
        for (auto& [name, binding] : bindings_) {
            auto it = current_values_.find(name);
            if (it != current_values_.end()) {
                if (!binding->update(it->second)) {
                    // Record parse failure (warning level)
                    std::string msg = "Failed to parse value for parameter '" + name + 
                                    "': '" + it->second + "'";
                    internal::log(LogLevel::Warning, msg);
                }
            } else {
                binding->apply_default();
            }
        }
        
        // Clear error on success
        last_error_ = ErrorInfo();
        
        return true;
    }
};

// ============================================================
// LiveTuner Class
// ============================================================

/**
 * @brief Live parameter tuning class (Low-level API)
 * 
 * Monitors file and reads values when changes occur.
 * 
 * @warning Thread Safety - Event-Driven Mode:
 * When using event-driven mode with get_async() callbacks, the callback
 * may be invoked from a BACKGROUND FILE-WATCHING THREAD, not your main thread.
 * 
 * This means you CANNOT safely:
 * - Call OpenGL/DirectX rendering APIs directly
 * - Update UI elements directly
 * - Access main-thread-only resources
 * 
 * For game development where main-thread execution is required,
 * consider using the Params class instead, which guarantees callbacks
 * run on the thread that calls update().
 * 
 * If you must use LiveTuner with rendering APIs, ensure proper
 * thread synchronization (e.g., command queues, mutexes).
 */
class LiveTuner {
public:
    struct FileCache {
        std::filesystem::file_time_type last_modify_time;
        std::chrono::steady_clock::time_point last_access;
        bool file_exists = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };

private:
    mutable std::mutex mtx_;
    std::string input_file_path_ = "params.txt";
    FileCache file_cache_{
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    };
    std::unique_ptr<internal::FileWatcher> file_watcher_;
    internal::FileWatcherConfig file_watcher_config_;
    internal::FileReadRetryConfig file_read_retry_config_;
    bool use_event_driven_ = true;
    
    // Error information
    ErrorInfo last_error_;

public:
    LiveTuner() = default;
    
    explicit LiveTuner(std::string_view file_path) : input_file_path_(file_path) {}
    
    /**
     * @brief Get file read retry configuration
     */
    const internal::FileReadRetryConfig& get_read_retry_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_read_retry_config_;
    }
    
    /**
     * @brief Change file read retry configuration
     * 
     * Used to avoid conflicts while editor is writing to file.
     * Especially effective on Windows.
     * 
     * Example:
     * @code
     * livetuner::LiveTuner tuner("params.txt");
     * 
     * // Customize retry settings
     * auto config = tuner.get_read_retry_config();
     * config.max_retries = 5;
     * config.retry_delay = std::chrono::milliseconds(10);
     * tuner.set_read_retry_config(config);
     * @endcode
     */
    void set_read_retry_config(const internal::FileReadRetryConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_read_retry_config_ = config;
    }
    
    LiveTuner(const LiveTuner&) = delete;
    LiveTuner& operator=(const LiveTuner&) = delete;
    LiveTuner(LiveTuner&&) noexcept;
    LiveTuner& operator=(LiveTuner&&) noexcept;
    
    ~LiveTuner();

    /**
     * @brief Set file path to monitor
     */
    void set_file(std::string_view file_path) {
        std::lock_guard<std::mutex> lock(mtx_);
        input_file_path_ = file_path;
        invalidate_cache();
    }
    
    std::string get_file() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return input_file_path_;
    }

    /**
     * @brief Get file watcher configuration
     */
    const internal::FileWatcherConfig& get_watcher_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_watcher_config_;
    }
    
    /**
     * @brief Change file watcher configuration
     * 
     * Example:
     * @code
     * livetuner::LiveTuner tuner("params.txt");
     * 
     * // If many file changes are expected
     * auto config = tuner.get_watcher_config();
     * config.buffer_size = 262144;  // 256KB
     * config.on_buffer_overflow = [](size_t old_size, size_t new_size) {
     *     if (new_size == 0) {
     *         std::cerr << "Buffer at maximum, events may be lost!" << std::endl;
     *     } else {
     *         std::cerr << "Buffer grown: " << old_size << " -> " << new_size << std::endl;
     *     }
     * };
     * tuner.set_watcher_config(config);
     * @endcode
     */
    void set_watcher_config(const internal::FileWatcherConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_watcher_config_ = config;
        file_watcher_config_.validate();
        
        // Update configuration if watcher already exists
        if (file_watcher_) {
            file_watcher_->set_config(file_watcher_config_);
        }
    }

    /**
     * @brief Enable/disable event-driven mode
     */
    void set_event_driven(bool enabled) {
        std::lock_guard<std::mutex> lock(mtx_);
        use_event_driven_ = enabled;
        if (file_watcher_) {
            file_watcher_->stop();
            file_watcher_.reset();
        }
    }
    
    bool is_event_driven() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return use_event_driven_;
    }
    
    static bool has_native_file_watch() {
        return internal::FileWatcher::has_native_support();
    }

    /**
     * @brief Get last error information
     * 
     * @return Error information (type == ErrorType::None if no error)
     * 
     * Example:
     * @code
     * livetuner::LiveTuner tuner("params.txt");
     * float speed = 1.0f;
     * if (!tuner.try_get(speed)) {
     *     auto error = tuner.last_error();
     *     if (error) {
     *         std::cerr << "Error: " << error.to_string() << std::endl;
     *     }
     * }
     * @endcode
     */
    ErrorInfo last_error() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return last_error_;
    }
    
    /**
     * @brief Check if there is an error
     */
    bool has_error() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return static_cast<bool>(last_error_);
    }
    
    /**
     * @brief Clear error
     */
    void clear_error() {
        std::lock_guard<std::mutex> lock(mtx_);
        last_error_ = ErrorInfo();
    }

    /**
     * @brief Try to read value immediately (non-blocking)
     * 
     * Updates value only when file has changed.
     * Optimal for game loops and frequent calls.
     * Built-in retry logic to avoid file write conflicts with editors.
     * 
     * @param value Variable to store read value
     * @return true if value was updated
     * 
     * @note Thread-safe: This method can be safely called from multiple threads.
     *       The file cache is protected by mutex to prevent data races.
     */
    template<typename T>
    bool try_get(T& value) {
        std::string input_path;
        internal::FileReadRetryConfig retry_config;
        
        auto now = std::chrono::steady_clock::now();
        
        // Phase 1: Check cache and get configuration (locked)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
            retry_config = file_read_retry_config_;
        }
        
        ensure_file_exists(input_path);
        
        // Phase 2: Get current file modification time (unlocked - filesystem operation)
        auto current_modify_time = internal::get_file_modify_time(input_path);
        
        // Phase 3: Re-check cache with modification time (locked)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            // Early return if within cache valid period and modification time is same
            if (file_cache_.file_exists && 
                (now - file_cache_.last_access) < FileCache::cache_duration &&
                current_modify_time == file_cache_.last_modify_time) {
                return false;
            }
        }
        
        // Phase 4: Read file (unlocked - I/O operation)
        ErrorInfo read_error;
        auto content_opt = internal::read_file_with_retry(input_path, retry_config, &read_error);
        if (!content_opt) {
            std::lock_guard<std::mutex> lock(mtx_);
            last_error_ = read_error;
            file_cache_.file_exists = false;
            file_cache_.last_access = now;
            return false;
        }
        
        // Phase 5: Parse content (unlocked - CPU operation)
        std::istringstream stream(*content_opt);
        std::string line;
        bool value_found = false;
        T parsed_value{};
        std::string failed_line;
        bool parse_error = false;
        
        while (std::getline(stream, line)) {
            line = internal::trim(line);
            
            if (!line.empty() && line[0] != '#') {
                if (internal::parse_value(line, parsed_value)) {
                    value_found = true;
                    break;
                } else {
                    // Parse failure - save for error reporting
                    failed_line = line;
                    parse_error = true;
                }
            }
        }
        
        // Phase 6: Update cache and state (locked)
        {
            std::lock_guard<std::mutex> lock(mtx_);
            
            if (value_found) {
                // On success, clear error and update cache
                value = std::move(parsed_value);
                file_cache_.last_modify_time = current_modify_time;
                file_cache_.last_access = now;
                file_cache_.file_exists = true;
                last_error_ = ErrorInfo();
                return true;
            }
            
            if (parse_error) {
                last_error_ = ErrorInfo(ErrorType::ParseError,
                                      "Failed to parse value from line: '" + failed_line + "'",
                                      input_path);
                internal::log(LogLevel::Warning, last_error_.to_string());
            } else {
                last_error_ = ErrorInfo(ErrorType::ParseError,
                                      "No valid value found in file",
                                      input_path);
                internal::log(LogLevel::Debug, last_error_.to_string());
            }
            
            file_cache_.last_modify_time = current_modify_time;
            file_cache_.last_access = now;
            file_cache_.file_exists = true;
        }
        
        return false;
    }

    /**
     * @brief Block until value is read
     * 
     * Waits until valid value is written to file.
     * For debugging and experimental use.
     */
    template<typename T>
    void get(T& value) {
        std::string input_path;
        bool event_driven;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
            event_driven = use_event_driven_;
        }
        
        ensure_file_exists(input_path);
        
        if (event_driven) {
            get_event_driven(value, input_path);
        } else {
            get_polling(value, input_path);
        }
    }

    /**
     * @brief Read value with timeout
     * 
     * @param value Variable to store read value
     * @param timeout Maximum wait time
     * @return true if value was read within time limit
     */
    template<typename T>
    bool get_timeout(T& value, std::chrono::milliseconds timeout) {
        std::string input_path;
        bool event_driven;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
            event_driven = use_event_driven_;
        }
        
        ensure_file_exists(input_path);
        
        if (event_driven) {
            return get_timeout_event_driven(value, input_path, timeout);
        } else {
            return get_timeout_polling(value, input_path, timeout);
        }
    }

    /**
     * @brief Read value asynchronously (future version)
     */
    template<typename T>
    std::future<T> get_async() {
        return std::async(std::launch::async, [this]() {
            T value{};
            get(value);
            return value;
        });
    }

    /**
     * @brief Read value asynchronously (callback version)
     */
    template<typename T, typename Callback>
    void get_async(Callback callback) {
        std::thread([this, callback = std::move(callback)]() {
            T value{};
            get(value);
            callback(value);
        }).detach();
    }

    /**
     * @brief Invalidate cache
     */
    void invalidate_cache() {
        file_cache_ = FileCache{
            std::filesystem::file_time_type::min(),
            std::chrono::steady_clock::time_point{},
            false
        };
    }

    /**
     * @brief Reset state (clear cache)
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        // Don't reset file path (maintain path set by set_file)
        if (file_watcher_) {
            file_watcher_->stop();
            file_watcher_.reset();
        }
        invalidate_cache();
    }

private:
    void ensure_file_exists(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            std::ofstream file(path);
            if (file) {
                file << "# Live Tuner parameters (edit values here)\n";
                file << "# Lines starting with # are comments\n";
            }
        }
    }

    template<typename T>
    void get_event_driven(T& value, const std::string& input_path) {
        auto watcher = std::make_unique<internal::FileWatcher>(file_watcher_config_);
        std::atomic<bool> file_changed{true};
        
        internal::FileReadRetryConfig retry_config;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            retry_config = file_read_retry_config_;
        }
        
        if (!watcher->start(input_path, [&file_changed] {
            file_changed.store(true);
        })) {
            std::lock_guard<std::mutex> lock(mtx_);
            last_error_ = ErrorInfo(ErrorType::WatcherError,
                                  "Failed to start file watcher, falling back to polling mode",
                                  input_path);
            internal::log(LogLevel::Warning, last_error_.to_string());
            internal::log(LogLevel::Info, 
                "LiveTuner: Event-driven file watching failed for '" + input_path + "'. "
                "Falling back to polling mode (100ms interval). "
                "This may occur due to: OS watcher resource limits, unsupported filesystem, "
                "or permission issues. Performance may be slightly reduced.");
            get_polling(value, input_path);
            return;
        }
        
        bool value_read = false;
        while (!value_read) {
            if (file_changed.load()) {
                file_changed.store(false);
                
                // Read file with retry logic
                ErrorInfo read_error;
                auto content_opt = internal::read_file_with_retry(input_path, retry_config, &read_error);
                if (content_opt) {
                    std::istringstream stream(*content_opt);
                    std::string line;
                    bool parsed = false;
                    while (std::getline(stream, line)) {
                        line = internal::trim(line);
                        
                        if (!line.empty() && line[0] != '#') {
                            if (internal::parse_value(line, value)) {
                                value_read = true;
                                parsed = true;
                                std::lock_guard<std::mutex> lock(mtx_);
                                last_error_ = ErrorInfo(); // Clear error on success
                                break;
                            }
                        }
                    }
                    if (!parsed) {
                        std::lock_guard<std::mutex> lock(mtx_);
                        last_error_ = ErrorInfo(ErrorType::ParseError,
                                              "No valid value found in file",
                                              input_path);
                        internal::log(LogLevel::Debug, last_error_.to_string());
                    }
                } else {
                    std::lock_guard<std::mutex> lock(mtx_);
                    last_error_ = read_error;
                }
            }
            
            if (!value_read) {
                watcher->wait_for_change(std::chrono::milliseconds(1000));
            }
        }
        
        watcher->stop();
    }
    
    template<typename T>
    void get_polling(T& value, const std::string& input_path) {
        internal::FileReadRetryConfig retry_config;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            retry_config = file_read_retry_config_;
        }
        
        bool value_read = false;
        
        while (!value_read) {
            // Read file with retry logic
            ErrorInfo read_error;
            auto content_opt = internal::read_file_with_retry(input_path, retry_config, &read_error);
            if (content_opt) {
                std::istringstream stream(*content_opt);
                std::string line;
                bool parsed = false;
                while (std::getline(stream, line)) {
                    line = internal::trim(line);
                    
                    if (!line.empty() && line[0] != '#') {
                        if (internal::parse_value(line, value)) {
                            value_read = true;
                            parsed = true;
                            std::lock_guard<std::mutex> lock(mtx_);
                            last_error_ = ErrorInfo(); // Clear error on success
                            break;
                        }
                    }
                }
                if (!parsed) {
                    std::lock_guard<std::mutex> lock(mtx_);
                    last_error_ = ErrorInfo(ErrorType::ParseError,
                                          "No valid value found in file",
                                          input_path);
                    internal::log(LogLevel::Debug, last_error_.to_string());
                }
            } else {
                std::lock_guard<std::mutex> lock(mtx_);
                last_error_ = read_error;
            }
            
            if (!value_read) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    template<typename T>
    bool get_timeout_event_driven(T& value, const std::string& input_path, 
                                  std::chrono::milliseconds timeout) {
        auto watcher = std::make_unique<internal::FileWatcher>(file_watcher_config_);
        std::atomic<bool> file_changed{true};
        
        if (!watcher->start(input_path, [&file_changed] {
            file_changed.store(true);
        })) {
            std::lock_guard<std::mutex> lock(mtx_);
            last_error_ = ErrorInfo(ErrorType::WatcherError,
                                  "Failed to start file watcher, falling back to polling mode",
                                  input_path);
            internal::log(LogLevel::Warning, last_error_.to_string());
            internal::log(LogLevel::Info, 
                "LiveTuner: Event-driven file watching failed for '" + input_path + "'. "
                "Falling back to polling mode with timeout. "
                "This may occur due to: OS watcher resource limits, unsupported filesystem, "
                "or permission issues.");
            return get_timeout_polling(value, input_path, timeout);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            );
            
            if (elapsed >= timeout) {
                watcher->stop();
                std::lock_guard<std::mutex> lock(mtx_);
                last_error_ = ErrorInfo(ErrorType::Timeout,
                                      "Timeout waiting for valid value",
                                      input_path);
                internal::log(LogLevel::Warning, last_error_.to_string());
                return false;
            }
            
            if (file_changed.load()) {
                file_changed.store(false);
                
                if (try_get(value)) {
                    watcher->stop();
                    return true;
                }
            }
            
            auto remaining = timeout - elapsed;
            watcher->wait_for_change(std::min(remaining, std::chrono::milliseconds(100)));
        }
    }
    
    template<typename T>
    bool get_timeout_polling(T& value, const std::string& input_path,
                             std::chrono::milliseconds timeout) {
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= timeout) {
                std::lock_guard<std::mutex> lock(mtx_);
                last_error_ = ErrorInfo(ErrorType::Timeout,
                                      "Timeout waiting for valid value",
                                      input_path);
                internal::log(LogLevel::Warning, last_error_.to_string());
                return false;
            }
            
            if (try_get(value)) {
                return true;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

// ============================================================
// Dependency Injection Support (Testable Design)
// ============================================================

/**
 * @brief Class for managing Tuner context with RAII
 * 
 * Use in large applications or unit tests when you want to avoid
 * global state. Automatically cleans up when leaving scope.
 * 
 * Example (test):
 * @code
 * TEST(MyTest, TestTuning) {
 *     livetuner::ScopedTunerContext ctx("test_params.txt");
 *     
 *     float value = 1.0f;
 *     ctx.tuner().try_get(value);
 *     // Automatic cleanup on test end
 * }
 * @endcode
 * 
 * Example (dependency injection):
 * @code
 * class GameEngine {
 *     livetuner::LiveTuner& tuner_;
 * public:
 *     explicit GameEngine(livetuner::LiveTuner& tuner) : tuner_(tuner) {}
 *     void update() {
 *         float speed;
 *         tuner_.try_get(speed);
 *     }
 * };
 * 
 * // Production
 * GameEngine engine(livetuner::get_default_tuner());
 * 
 * // Test
 * livetuner::LiveTuner test_tuner("test.txt");
 * GameEngine engine(test_tuner);
 * @endcode
 */
class ScopedTunerContext {
public:
    explicit ScopedTunerContext(std::string_view file_path = "params.txt")
        : tuner_(std::make_unique<LiveTuner>(file_path)) {}
    
    ~ScopedTunerContext() = default;
    
    ScopedTunerContext(const ScopedTunerContext&) = delete;
    ScopedTunerContext& operator=(const ScopedTunerContext&) = delete;
    ScopedTunerContext(ScopedTunerContext&&) = default;
    ScopedTunerContext& operator=(ScopedTunerContext&&) = default;
    
    LiveTuner& tuner() { return *tuner_; }
    const LiveTuner& tuner() const { return *tuner_; }
    
private:
    std::unique_ptr<LiveTuner> tuner_;
};

/**
 * @brief Class for managing Params context with RAII
 * 
 * Example (test):
 * @code
 * TEST(MyTest, TestParams) {
 *     livetuner::ScopedParamsContext ctx("test_config.json");
 *     
 *     float speed = 1.0f;
 *     ctx.params().bind("speed", speed, 1.0f);
 *     ctx.params().update();
 *     // Automatic cleanup on test end
 * }
 * @endcode
 */
class ScopedParamsContext {
public:
    explicit ScopedParamsContext(std::string_view file_path = "params.json",
                                  FileFormat format = FileFormat::Auto)
        : params_(std::make_unique<Params>(file_path, format)) {}
    
    ~ScopedParamsContext() = default;
    
    ScopedParamsContext(const ScopedParamsContext&) = delete;
    ScopedParamsContext& operator=(const ScopedParamsContext&) = delete;
    ScopedParamsContext(ScopedParamsContext&&) = default;
    ScopedParamsContext& operator=(ScopedParamsContext&&) = default;
    
    Params& params() { return *params_; }
    const Params& params() const { return *params_; }
    
private:
    std::unique_ptr<Params> params_;
};

// ============================================================
// Global Instances (For Personal Development and Small Projects)
// ============================================================

namespace internal {

/**
 * @brief Global instance management class
 * 
 * Internal implementation class to allow complete reset of global
 * state during testing. Not normally used directly.
 */
class GlobalInstanceManager {
public:
    static GlobalInstanceManager& instance() {
        static GlobalInstanceManager mgr;
        return mgr;
    }
    
    LiveTuner& get_tuner() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!tuner_) {
            tuner_ = std::make_unique<LiveTuner>();
        }
        return *tuner_;
    }
    
    Params& get_params() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!params_) {
            params_ = std::make_unique<Params>("config.json");
        }
        return *params_;
    }
    
    /**
     * @brief Completely reset global LiveTuner
     * 
     * Creates new instance and clears all state.
     * Use in unit test setUp/tearDown.
     */
    void reset_tuner() {
        std::lock_guard<std::mutex> lock(mtx_);
        tuner_.reset();
    }
    
    /**
     * @brief Completely reset global Params
     * 
     * Creates new instance and clears all bindings and state.
     * Use in unit test setUp/tearDown.
     */
    void reset_params() {
        std::lock_guard<std::mutex> lock(mtx_);
        params_.reset();
    }
    
    /**
     * @brief Reset all global instances
     */
    void reset_all() {
        std::lock_guard<std::mutex> lock(mtx_);
        tuner_.reset();
        params_.reset();
    }

private:
    GlobalInstanceManager() = default;
    
    std::mutex mtx_;
    std::unique_ptr<LiveTuner> tuner_;
    std::unique_ptr<Params> params_;
};

} // namespace internal

/**
 * @brief Get default LiveTuner instance
 * 
 * @note For large applications or tests, recommend using ScopedTunerContext
 *       or LiveTuner class directly.
 */
inline LiveTuner& get_default_tuner() {
    return internal::GlobalInstanceManager::instance().get_tuner();
}

/**
 * @brief Get default Params instance
 * 
 * @note For large applications or tests, recommend using ScopedParamsContext
 *       or Params class directly.
 */
inline Params& get_default_params() {
    return internal::GlobalInstanceManager::instance().get_params();
}

// ============================================================
// Convenient Global Functions (For Personal Development and Small Projects)
// ============================================================

/**
 * @brief Set file to monitor
 * @param file_path Parameter file path (default: "params.txt")
 * 
 * @note For large applications, recommend using LiveTuner class directly
 *       with dependency injection pattern.
 */
inline void tune_init(std::string_view file_path) {
    get_default_tuner().set_file(file_path);
}

/**
 * @brief Try to read value immediately (non-blocking)
 * 
 * Example:
 * @code
 * float speed = 1.0f;
 * while (running) {
 *     tune_try(speed);  // Update if changed
 *     player.move(speed);
 * }
 * @endcode
 * 
 * @note For large applications, recommend using LiveTuner::try_get() directly
 *       with dependency injection pattern.
 */
template<typename T>
inline bool tune_try(T& value) {
    return get_default_tuner().try_get(value);
}

/**
 * @brief Block until value is read
 */
template<typename T>
inline void tune(T& value) {
    get_default_tuner().get(value);
}

/**
 * @brief Read value with timeout
 */
template<typename T>
inline bool tune_timeout(T& value, std::chrono::milliseconds timeout) {
    return get_default_tuner().get_timeout(value, timeout);
}

/**
 * @brief Read value asynchronously (future version)
 */
template<typename T>
inline std::future<T> tune_async() {
    return get_default_tuner().get_async<T>();
}

/**
 * @brief Read value asynchronously (callback version)
 */
template<typename T, typename Callback>
inline void tune_async(Callback callback) {
    get_default_tuner().get_async<T>(std::move(callback));
}

/**
 * @brief Set event-driven mode
 */
inline void tune_set_event_driven(bool enabled) {
    get_default_tuner().set_event_driven(enabled);
}

inline bool tune_is_event_driven() {
    return get_default_tuner().is_event_driven();
}

inline bool tune_has_native_file_watch() {
    return LiveTuner::has_native_file_watch();
}

/**
 * @brief Reset state (for testing)
 */
inline void tune_reset() {
    get_default_tuner().reset();
}

#ifdef LIVETUNER_ENABLE_TEST_SUPPORT
/**
 * @brief Completely reset global LiveTuner
 * 
 * More powerful reset than tune_reset(), recreates instance itself.
 * Use when complete separation between tests is needed.
 * 
 * @note Requires LIVETUNER_ENABLE_TEST_SUPPORT to be defined.
 */
inline void reset_global_tuner() {
    internal::GlobalInstanceManager::instance().reset_tuner();
}
#endif // LIVETUNER_ENABLE_TEST_SUPPORT

// ============================================================
// Convenient Global Functions for Params
// ============================================================

/**
 * @brief Set file for global Params
 * 
 * @note For large applications, recommend using the Params class directly
 *       with dependency injection pattern.
 */
inline void params_init(std::string_view file_path, FileFormat format = FileFormat::Auto) {
    get_default_params().set_file(file_path, format);
}

/**
 * @brief Bind variable to global Params
 * 
 * @note For large applications, recommend using Params::bind() directly
 *       with dependency injection pattern.
 */
template<typename T>
inline void params_bind(const std::string& name, T& variable, T default_value = T{}) {
    get_default_params().bind(name, variable, default_value);
}

/**
 * @brief Update global Params (non-blocking)
 */
inline bool params_update() {
    return get_default_params().update();
}

/**
 * @brief Start watching global Params
 */
inline void params_watch() {
    get_default_params().start_watching();
}

/**
 * @brief Poll global Params for changes
 */
inline bool params_poll() {
    return get_default_params().poll();
}

/**
 * @brief Get value from global Params
 */
template<typename T>
inline std::optional<T> params_get(const std::string& name) {
    return get_default_params().get<T>(name);
}

/**
 * @brief Get value from global Params (with default value)
 */
template<typename T>
inline T params_get_or(const std::string& name, T default_value) {
    return get_default_params().get_or(name, default_value);
}

/**
 * @brief Set change callback for global Params
 */
inline void params_on_change(std::function<void()> callback) {
    get_default_params().on_change(std::move(callback));
}

/**
 * @brief Reset global Params
 */
inline void params_reset() {
    get_default_params().unbind_all();
    get_default_params().invalidate_cache();
}

#ifdef LIVETUNER_ENABLE_TEST_SUPPORT
/**
 * @brief Completely reset global Params
 * 
 * More powerful reset than params_reset(), recreates instance itself.
 * Use when complete separation between tests is needed.
 * 
 * @note Requires LIVETUNER_ENABLE_TEST_SUPPORT to be defined.
 */
inline void reset_global_params() {
    internal::GlobalInstanceManager::instance().reset_params();
}

/**
 * @brief Reset all global instances
 * 
 * @note Requires LIVETUNER_ENABLE_TEST_SUPPORT to be defined.
 */
inline void reset_all_globals() {
    internal::GlobalInstanceManager::instance().reset_all();
}
#endif // LIVETUNER_ENABLE_TEST_SUPPORT

// ============================================================
// nlohmann/json Support (Optional Feature)
// ============================================================

#ifdef LIVETUNER_USE_NLOHMANN_JSON

// nlohmann/json include
// Users must #include <nlohmann/json.hpp> beforehand
#ifndef NLOHMANN_JSON_VERSION_MAJOR
#error "LIVETUNER_USE_NLOHMANN_JSON is defined but nlohmann/json.hpp is not included. Please #include <nlohmann/json.hpp> before LiveTuner.h"
#endif

/**
 * @brief Parameter management class for nlohmann/json
 * 
 * To use this class, the following steps are required:
 * 1. Add nlohmann/json to your project
 * 2. Define #define LIVETUNER_USE_NLOHMANN_JSON
 * 3. Include #include <nlohmann/json.hpp>
 * 4. Include #include "LiveTuner.h"
 * 
 * Features:
 * - Loading nested JSON objects
 * - Array support
 * - Type-safe value retrieval
 * - JSON schema validation
 * - Custom type serialization support
 * 
 * Example:
 * @code
 * #define LIVETUNER_USE_NLOHMANN_JSON
 * #include <nlohmann/json.hpp>
 * #include "LiveTuner.h"
 * 
 * using json = nlohmann::json;
 * 
 * livetuner::NlohmannParams params("config.json");
 * 
 * // Basic usage
 * float speed = params.get<float>("player.speed", 1.0f);
 * std::string name = params.get<std::string>("player.name", "Player");
 * 
 * // Get array
 * auto colors = params.get<std::vector<int>>("colors", {255, 0, 0});
 * 
 * // Get nested object
 * auto player_data = params.get_json("player");
 * 
 * while (running) {
 *     if (params.update()) {
 *         // Process when JSON changes
 *         speed = params.get<float>("player.speed", 1.0f);
 *     }
 * }
 * @endcode
 * 
 * JSON file example:
 * @code
 * {
 *   "player": {
 *     "name": "Hero",
 *     "speed": 2.5,
 *     "position": [10.0, 20.0, 30.0]
 *   },
 *   "colors": [255, 128, 0],
 *   "debug": true
 * }
 * @endcode
 */
class NlohmannParams {
public:
    using json = nlohmann::json;
    using ErrorCallback = std::function<void(const ErrorInfo&)>;

    /**
     * @brief Constructor
     * @param file_path Path to JSON file to watch
     */
    explicit NlohmannParams(const std::string& file_path)
        : file_path_(file_path)
        , error_callback_(nullptr)
    {
        // Initial load
        load();
        
        // Initialize FileWatcher
        internal::FileWatcherConfig config;
        watcher_ = std::make_unique<internal::FileWatcher>(config);
        watcher_->start(file_path, []() {
            // Callback not needed (changes checked in check())
        });
    }

    /**
     * @brief Destructor
     */
    ~NlohmannParams();

    // Copy and move prohibited
    NlohmannParams(const NlohmannParams&) = delete;
    NlohmannParams& operator=(const NlohmannParams&) = delete;
    NlohmannParams(NlohmannParams&&) = delete;
    NlohmannParams& operator=(NlohmannParams&&) = delete;

    /**
     * @brief Check file changes and update
     * @return true if updated
     */
    bool update() {
        // Check for changes non-blocking (timeout 0ms)
        if (!watcher_->wait_for_change(std::chrono::milliseconds(0))) {
            return false;
        }
        return load();
    }

    /**
     * @brief Get value (specify JSON path)
     * @tparam T Type of value to retrieve
     * @param json_path JSON path (e.g., "player.speed" or "colors[0]")
     * @param default_value Default value
     * @return Retrieved value (default value on error)
     */
    template<typename T>
    T get(const std::string& json_path, const T& default_value = T{}) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            json value = get_value_by_path(json_, json_path);
            if (value.is_null()) {
                return default_value;
            }
            return value.get<T>();
        } catch (const std::exception& e) {
            handle_error(ErrorType::ParseError, 
                        "Failed to get value at '" + json_path + "': " + e.what());
            return default_value;
        }
    }

    /**
     * @brief Get JSON object
     * @param json_path JSON path (root if omitted)
     * @return JSON object
     */
    json get_json(const std::string& json_path = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (json_path.empty()) {
            return json_;
        }
        
        try {
            return get_value_by_path(json_, json_path);
        } catch (const std::exception& e) {
            handle_error(ErrorType::ParseError, 
                        "Failed to get JSON at '" + json_path + "': " + e.what());
            return json{};
        }
    }

    /**
     * @brief Check if value exists
     * @param json_path JSON path
     * @return true if exists
     */
    bool has(const std::string& json_path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            json value = get_value_by_path(json_, json_path);
            return !value.is_null();
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Set error callback
     * @param callback Callback function called on error
     */
    void set_error_callback(ErrorCallback callback) {
        error_callback_ = std::move(callback);
    }

    /**
     * @brief Get last error
     * @return Error information
     */
    const ErrorInfo& last_error() const {
        return last_error_;
    }

    /**
     * @brief Save current JSON to file
     * @param pretty Whether to format output
     * @return true if successful
     */
    bool save(bool pretty = true) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ofstream file(file_path_);
            if (!file) {
                handle_error(ErrorType::FileAccessDenied, 
                           "Failed to open file for writing: " + file_path_);
                return false;
            }
            
            if (pretty) {
                file << json_.dump(2);  // 2-space indent
            } else {
                file << json_.dump();
            }
            
            return true;
        } catch (const std::exception& e) {
            handle_error(ErrorType::FileReadError, 
                        "Failed to save JSON: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Set JSON (change value from program)
     * @param json_path JSON path
     * @param value Value to set
     * @return true if successful
     */
    template<typename T>
    bool set(const std::string& json_path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            set_value_by_path(json_, json_path, value);
            return true;
        } catch (const std::exception& e) {
            handle_error(ErrorType::ParseError, 
                        "Failed to set value at '" + json_path + "': " + e.what());
            return false;
        }
    }

    /**
     * @brief Get file path
     */
    const std::string& file_path() const {
        return file_path_;
    }

    /**
     * @brief For debugging: Get entire current JSON as string
     */
    std::string dump(int indent = 2) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return json_.dump(indent);
    }

private:
    /**
     * @brief Load JSON file
     */
    bool load() {
        try {
            std::ifstream file(file_path_);
            if (!file) {
                handle_error(ErrorType::FileNotFound, 
                           "File not found: " + file_path_);
                return false;
            }
            
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Empty file check
            file.seekg(0, std::ios::end);
            if (file.tellg() == 0) {
                json_ = json::object();  // Initialize as empty object
                return true;
            }
            file.seekg(0, std::ios::beg);
            
            // JSON parse
            try {
                file >> json_;
            } catch (const json::parse_error& e) {
                handle_error(ErrorType::ParseError, 
                           "JSON parse error: " + std::string(e.what()));
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            handle_error(ErrorType::FileReadError, 
                        "Failed to read file: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Get value by JSON path
     * @param j JSON object
     * @param path Path (dot-separated, e.g., "player.speed")
     * @return Value (null if not found)
     */
    static json get_value_by_path(const json& j, const std::string& path) {
        if (path.empty()) {
            return j;
        }
        
        json current = j;
        std::string token;
        std::istringstream path_stream(path);
        
        while (std::getline(path_stream, token, '.')) {
            // Array access support: "array[0]"
            size_t bracket_pos = token.find('[');
            if (bracket_pos != std::string::npos) {
                std::string key = token.substr(0, bracket_pos);
                size_t bracket_end = token.find(']', bracket_pos);
                if (bracket_end == std::string::npos) {
                    throw std::runtime_error("Invalid array syntax in path: " + path);
                }
                
                std::string index_str = token.substr(bracket_pos + 1, bracket_end - bracket_pos - 1);
                int index = std::stoi(index_str);
                
                if (!key.empty()) {
                    if (!current.contains(key) || !current[key].is_array()) {
                        return json{};
                    }
                    current = current[key];
                }
                
                if (index < 0 || static_cast<size_t>(index) >= current.size()) {
                    return json{};
                }
                current = current[index];
            } else {
                if (!current.is_object() || !current.contains(token)) {
                    return json{};
                }
                current = current[token];
            }
        }
        
        return current;
    }

    /**
     * @brief Set value by JSON path
     * @param j JSON object
     * @param path Path (dot-separated)
     * @param value Value to set
     */
    template<typename T>
    static void set_value_by_path(json& j, const std::string& path, const T& value) {
        if (path.empty()) {
            j = value;
            return;
        }
        
        json* current = &j;
        std::string token;
        std::istringstream path_stream(path);
        std::vector<std::string> tokens;
        
        while (std::getline(path_stream, token, '.')) {
            tokens.push_back(token);
        }
        
        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            const auto& tok = tokens[i];
            
            if (!current->is_object()) {
                *current = json::object();
            }
            
            if (!current->contains(tok)) {
                (*current)[tok] = json::object();
            }
            
            current = &(*current)[tok];
        }
        
        if (!current->is_object()) {
            *current = json::object();
        }
        
        (*current)[tokens.back()] = value;
    }

    /**
     * @brief Handle error
     */
    void handle_error(ErrorType type, const std::string& message) const {
        last_error_ = ErrorInfo(type, message, file_path_);
        
        if (error_callback_) {
            error_callback_(last_error_);
        }
    }

private:
    std::string file_path_;
    std::unique_ptr<internal::FileWatcher> watcher_;
    mutable json json_;
    mutable std::mutex mutex_;
    mutable ErrorInfo last_error_;
    ErrorCallback error_callback_;
};

/**
 * @brief Automatic binding helper using nlohmann/json
 * 
 * Example:
 * @code
 * #define LIVETUNER_USE_NLOHMANN_JSON
 * #include <nlohmann/json.hpp>
 * #include "LiveTuner.h"
 * 
 * livetuner::NlohmannBinder binder("config.json");
 * 
 * float speed;
 * std::string name;
 * std::vector<float> position;
 * 
 * binder.bind("player.speed", speed, 1.0f);
 * binder.bind("player.name", name, std::string("Player"));
 * binder.bind("player.position", position, {0.0f, 0.0f, 0.0f});
 * 
 * while (running) {
 *     if (binder.update()) {
 *         // All bound variables are automatically updated
 *     }
 * }
 * @endcode
 */
class NlohmannBinder {
public:
    using json = nlohmann::json;

    explicit NlohmannBinder(const std::string& file_path)
        : params_(file_path)
    {}

    /**
     * @brief Bind variable
     * @tparam T Variable type
     * @param json_path JSON path
     * @param variable Reference to variable to bind
     * @param default_value Default value
     */
    template<typename T>
    void bind(const std::string& json_path, T& variable, const T& default_value = T{}) {
        // Set initial value
        variable = params_.get<T>(json_path, default_value);
        
        // Save binding information
        bindings_.emplace_back([this, json_path, &variable, default_value]() {
            variable = params_.get<T>(json_path, default_value);
        });
    }

    /**
     * @brief Check for updates and update all bound variables
     * @return true if updated
     */
    bool update() {
        if (!params_.update()) {
            return false;
        }
        
        // Update all bound variables
        for (auto& binding : bindings_) {
            binding();
        }
        
        return true;
    }

    /**
     * @brief Get the underlying NlohmannParams
     */
    NlohmannParams& params() {
        return params_;
    }

    const NlohmannParams& params() const {
        return params_;
    }

private:
    NlohmannParams params_;
    std::vector<std::function<void()>> bindings_;
};

#endif // LIVETUNER_USE_NLOHMANN_JSON

} // namespace livetuner

// ============================================================
// Test Support (Optional - Include LiveTunerTest.h)
// ============================================================

/**
 * If LIVETUNER_ENABLE_TEST_SUPPORT is defined, automatically include
 * LiveTunerTest.h which provides:
 * - TestFixture: RAII helper for resetting global state
 * - ITuner/IParams: Interfaces for dependency injection and mocking
 * - TunerAdapter/ParamsAdapter: Adapters for DI patterns
 * - ScopedContext: Thread-local context for isolated testing
 * - TunerFactory/ParamsFactory: Factory pattern for instance creation
 * - get_context_tuner()/get_context_params(): Context-aware access
 * 
 * For test code, define before including:
 *   #define LIVETUNER_ENABLE_TEST_SUPPORT
 *   #include "LiveTuner.h"
 * 
 * Or include LiveTunerTest.h directly:
 *   #include "LiveTuner.h"
 *   #include "LiveTunerTest.h"
 */
#ifdef LIVETUNER_ENABLE_TEST_SUPPORT
#include "LiveTunerTest.h"
#endif

/*
 * ============================================================================
 * LICENSE INFORMATION
 * ============================================================================
 * 
 * This library (LiveTuner) includes picojson, which is licensed under the
 * 2-clause BSD License (reproduced below).
 * 
 * ----------------------------------------------------------------------------
 * picojson - a C++ JSON parser / serializer
 * Copyright 2009-2010 Cybozu Labs, Inc.
 * Copyright 2011-2014 Kazuho Oku
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */ 

// ============================================================================
// IMPLEMENTATION SECTION
// ============================================================================
//
// Define LIVETUNER_IMPLEMENTATION in exactly ONE source file before including
// this header to provide the implementation of platform-specific features.
//
// Example:
//   // In main.cpp or a dedicated livetuner.cpp:
//   #define LIVETUNER_IMPLEMENTATION
//   #include "LiveTuner.h"
//
// Without LIVETUNER_IMPLEMENTATION, FileWatcher will use polling mode only.
// ============================================================================

#ifdef LIVETUNER_IMPLEMENTATION

// ============================================================
// MSVC warning suppression for implementation block
// ============================================================
// Suppress all warnings in the implementation section to avoid
// noise from Windows.h and other platform headers when using
// high warning levels (/W4 or /Wall)
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif

// ============================================================
// Platform-specific headers (only in implementation)
// ============================================================

#ifdef _WIN32
// Settings to prevent Windows.h macro pollution
#ifndef LIVETUNER_NO_WIN32_LEAN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define LIVETUNER_IMPL_DEFINED_WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#define LIVETUNER_IMPL_DEFINED_VC_EXTRALEAN
#endif
#endif

#ifndef LIVETUNER_NO_NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#define LIVETUNER_IMPL_DEFINED_NOMINMAX
#endif
#endif

#include <windows.h>

// Restore defined macros
#ifdef LIVETUNER_IMPL_DEFINED_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#undef LIVETUNER_IMPL_DEFINED_WIN32_LEAN_AND_MEAN
#endif
#ifdef LIVETUNER_IMPL_DEFINED_VC_EXTRALEAN
#undef VC_EXTRALEAN
#undef LIVETUNER_IMPL_DEFINED_VC_EXTRALEAN
#endif
#ifdef LIVETUNER_IMPL_DEFINED_NOMINMAX
#undef NOMINMAX
#undef LIVETUNER_IMPL_DEFINED_NOMINMAX
#endif

#elif defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <cerrno>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#endif

namespace livetuner {
namespace internal {

// ============================================================
// RAII Resource Wrappers Implementation
// ============================================================

#ifdef _WIN32
struct HandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

struct InvalidHandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;
using UniqueInvalidHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, InvalidHandleDeleter>;

inline UniqueHandle make_unique_handle(HANDLE h) {
    return UniqueHandle(h);
}

inline UniqueInvalidHandle make_unique_invalid_handle(HANDLE h) {
    return UniqueInvalidHandle(h);
}
#endif // _WIN32

#ifdef __linux__
class UniqueFd {
public:
    UniqueFd() : fd_(-1) {}
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { reset(); }
    
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    int get() const noexcept { return fd_; }
    int release() noexcept {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }
    
    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = fd;
    }
    
    explicit operator bool() const noexcept { return fd_ >= 0; }
    
private:
    int fd_;
};
#endif // __linux__

// ============================================================
// FileWatcher Implementation (PIMPL)
// ============================================================

struct FileWatcher::Impl {
#ifdef _WIN32
    UniqueInvalidHandle dir_handle;
    UniqueHandle stop_event;
    std::atomic<size_t> current_buffer_size{0};
#endif

#ifdef __linux__
    UniqueFd inotify_fd;
    int dir_watch_fd = -1;
    UniqueFd pipe_read_fd;
    UniqueFd pipe_write_fd;
    std::string target_filename;
    std::filesystem::path dir_path;
#endif

#ifdef __APPLE__
    FSEventStreamRef stream = nullptr;
    dispatch_queue_t queue = nullptr;
#endif
};

// Constructor
inline FileWatcher::FileWatcher() : impl_(std::make_unique<Impl>()) {}

inline FileWatcher::FileWatcher(const FileWatcherConfig& config)
    : impl_(std::make_unique<Impl>()), config_(config) {
    config_.validate();
}

inline FileWatcher::~FileWatcher() {
    stop();
}

inline FileWatcher::FileWatcher(FileWatcher&& other) noexcept
    : impl_(std::move(other.impl_))
    , file_path_(std::move(other.file_path_))
    , callback_(std::move(other.callback_))
    , running_(other.running_.load())
    , change_detected_(other.change_detected_.load())
    , watcher_thread_(std::move(other.watcher_thread_))
    , config_(std::move(other.config_)) {
    other.running_.store(false);
    other.change_detected_.store(false);
}

inline FileWatcher& FileWatcher::operator=(FileWatcher&& other) noexcept {
    if (this != &other) {
        stop();
        impl_ = std::move(other.impl_);
        file_path_ = std::move(other.file_path_);
        callback_ = std::move(other.callback_);
        running_.store(other.running_.load());
        change_detected_.store(other.change_detected_.load());
        watcher_thread_ = std::move(other.watcher_thread_);
        config_ = std::move(other.config_);
        other.running_.store(false);
        other.change_detected_.store(false);
    }
    return *this;
}

inline void FileWatcher::set_config(const FileWatcherConfig& config) {
    if (!running_.load()) {
        config_ = config;
        config_.validate();
    }
}

inline bool FileWatcher::start(const std::filesystem::path& file_path, ChangeCallback callback) {
    if (running_.load()) {
        stop();
    }

    file_path_ = file_path;
    callback_ = std::move(callback);
    running_.store(true);

    return start_native();
}

inline void FileWatcher::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    
    {
        std::lock_guard<std::mutex> lock(cv_mtx_);
        change_detected_.store(true);
    }
    cv_.notify_all();

    stop_native();

    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
}

inline bool FileWatcher::wait_for_change(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(cv_mtx_);
    
    if (change_detected_.load()) {
        change_detected_.store(false);
        return true;
    }
    
    bool result = cv_.wait_for(lock, timeout, [this] {
        return change_detected_.load() || !running_.load();
    });
    
    if (result && change_detected_.load()) {
        change_detected_.store(false);
        return true;
    }
    return false;
}

inline void FileWatcher::wait_for_change() {
    std::unique_lock<std::mutex> lock(cv_mtx_);
    cv_.wait(lock, [this] {
        return change_detected_.load() || !running_.load();
    });
    change_detected_.store(false);
}

inline bool FileWatcher::is_running() const {
    return running_.load();
}

inline bool FileWatcher::has_native_support() {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

inline void FileWatcher::notify_change() {
    {
        std::lock_guard<std::mutex> lock(cv_mtx_);
        change_detected_.store(true);
    }
    cv_.notify_all();
    
    if (callback_) {
        callback_();
    }
}

inline bool FileWatcher::start_polling() {
    watcher_thread_ = std::thread([this] { watch_polling(); });
    return true;
}

inline void FileWatcher::watch_polling() {
    auto last_modify_time = std::filesystem::file_time_type::min();
    
    std::chrono::milliseconds poll_interval{50};
    constexpr std::chrono::milliseconds min_interval{10};
    constexpr std::chrono::milliseconds max_interval{500};
    int no_change_count = 0;

    while (running_.load()) {
        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(file_path_, ec);
        
        if (!ec && current_time != last_modify_time) {
            last_modify_time = current_time;
            notify_change();
            poll_interval = min_interval;
            no_change_count = 0;
        } else {
            ++no_change_count;
            if (no_change_count > 10 && poll_interval < max_interval) {
                poll_interval = std::min(poll_interval * 2, max_interval);
            }
        }

        std::unique_lock<std::mutex> lock(cv_mtx_);
        cv_.wait_for(lock, poll_interval, [this] {
            return !running_.load();
        });
    }
}

// ============================================================
// Platform-specific FileWatcher Implementation
// ============================================================

#ifdef _WIN32
inline bool FileWatcher::start_native() {
    impl_->current_buffer_size.store(config_.buffer_size);
    
    auto dir_path = file_path_.parent_path();
    if (dir_path.empty()) {
        dir_path = ".";
    }

    auto dir_handle = make_unique_invalid_handle(CreateFileW(
        dir_path.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    ));

    if (dir_handle.get() == INVALID_HANDLE_VALUE) {
        return start_polling();
    }

    auto stop_event = make_unique_handle(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    if (!stop_event) {
        return start_polling();
    }

    impl_->dir_handle = std::move(dir_handle);
    impl_->stop_event = std::move(stop_event);

    watcher_thread_ = std::thread([this] {
        size_t buffer_size = impl_->current_buffer_size.load();
        std::vector<BYTE> buffer(buffer_size);
        OVERLAPPED overlapped{};
        auto overlapped_event = make_unique_handle(CreateEvent(nullptr, TRUE, FALSE, nullptr));
        if (!overlapped_event) {
            return;
        }
        overlapped.hEvent = overlapped_event.get();

        auto filename = file_path_.filename().wstring();

        while (running_.load()) {
            ResetEvent(overlapped.hEvent);

            BOOL result = ReadDirectoryChangesW(
                impl_->dir_handle.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                nullptr,
                &overlapped,
                nullptr
            );

            if (!result) {
                DWORD error = GetLastError();
                if (error == ERROR_NOTIFY_ENUM_DIR) {
                    // Buffer overflow - handle and continue
                    if (config_.auto_grow_buffer) {
                        size_t current_size = buffer.size();
                        size_t new_size = std::min(current_size * 2, config_.max_buffer_size);
                        if (new_size > current_size) {
                            try {
                                buffer.resize(new_size);
                                impl_->current_buffer_size.store(new_size);
                                if (config_.on_buffer_overflow) {
                                    config_.on_buffer_overflow(current_size, new_size);
                                }
                            } catch (...) {}
                        }
                    }
                    continue;
                }
                break;
            }

            HANDLE handles[] = { overlapped.hEvent, impl_->stop_event.get() };
            DWORD wait_result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (wait_result == WAIT_OBJECT_0) {
                DWORD bytes_returned = 0;
                if (GetOverlappedResult(impl_->dir_handle.get(), &overlapped, &bytes_returned, FALSE)) {
                    if (bytes_returned == 0) {
                        notify_change();
                        continue;
                    }
                    
                    auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
                    do {
                        std::wstring changed_filename(info->FileName, info->FileNameLength / sizeof(WCHAR));
                        if (changed_filename == filename) {
                            notify_change();
                            break;
                        }
                        if (info->NextEntryOffset == 0) break;
                        info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                            reinterpret_cast<BYTE*>(info) + info->NextEntryOffset
                        );
                    } while (true);
                }
            } else {
                break;
            }
        }
    });
    return true;
}

inline void FileWatcher::stop_native() {
    if (impl_->stop_event) {
        SetEvent(impl_->stop_event.get());
    }
    if (impl_->dir_handle) {
        CancelIo(impl_->dir_handle.get());
    }
    impl_->dir_handle.reset();
    impl_->stop_event.reset();
}

#elif defined(__linux__)
inline bool FileWatcher::start_native() {
    UniqueFd inotify_fd(inotify_init1(IN_NONBLOCK));
    if (!inotify_fd) {
        return start_polling();
    }

    impl_->dir_path = file_path_.parent_path();
    if (impl_->dir_path.empty()) {
        impl_->dir_path = ".";
    }
    impl_->target_filename = file_path_.filename().string();

    int dir_watch_fd = inotify_add_watch(
        inotify_fd.get(),
        impl_->dir_path.c_str(),
        IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM
    );

    if (dir_watch_fd < 0) {
        return start_polling();
    }

    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        inotify_rm_watch(inotify_fd.get(), dir_watch_fd);
        return start_polling();
    }

    impl_->inotify_fd = std::move(inotify_fd);
    impl_->dir_watch_fd = dir_watch_fd;
    impl_->pipe_read_fd.reset(pipe_fds[0]);
    impl_->pipe_write_fd.reset(pipe_fds[1]);

    watcher_thread_ = std::thread([this] {
        constexpr size_t event_size = sizeof(struct inotify_event);
        constexpr size_t buffer_size = 1024 * (event_size + NAME_MAX + 1);
        std::vector<char> buffer(buffer_size);

        struct pollfd fds[2];
        fds[0].fd = impl_->inotify_fd.get();
        fds[0].events = POLLIN;
        fds[1].fd = impl_->pipe_read_fd.get();
        fds[1].events = POLLIN;

        while (running_.load()) {
            int poll_result = poll(fds, 2, -1);
            
            if (poll_result < 0) {
                if (errno == EINTR) continue;
                break;
            }

            if (fds[1].revents & POLLIN) {
                break;
            }

            if (fds[0].revents & POLLIN) {
                ssize_t len = read(impl_->inotify_fd.get(), buffer.data(), buffer_size);
                if (len < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    break;
                }
                
                if (len > 0) {
                    size_t i = 0;
                    bool should_notify = false;
                    
                    while (i < static_cast<size_t>(len)) {
                        const auto* event = reinterpret_cast<const struct inotify_event*>(buffer.data() + i);
                        
                        if (event->len > 0) {
                            std::string event_name(event->name);
                            
                            if (event_name == impl_->target_filename) {
                                if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                                    should_notify = true;
                                }
                                else if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    should_notify = true;
                                }
                            }
                        }
                        
                        i += sizeof(struct inotify_event) + event->len;
                    }
                    
                    if (should_notify) {
                        notify_change();
                    }
                }
            }
        }
    });
    return true;
}

inline void FileWatcher::stop_native() {
    if (impl_->pipe_write_fd) {
        char c = 'x';
        [[maybe_unused]] auto _ = write(impl_->pipe_write_fd.get(), &c, 1);
    }
    if (impl_->dir_watch_fd >= 0 && impl_->inotify_fd) {
        inotify_rm_watch(impl_->inotify_fd.get(), impl_->dir_watch_fd);
        impl_->dir_watch_fd = -1;
    }
    impl_->inotify_fd.reset();
    impl_->pipe_read_fd.reset();
    impl_->pipe_write_fd.reset();
}

#elif defined(__APPLE__)
namespace {
    void fsevents_callback_impl(
        ConstFSEventStreamRef,
        void* info,
        size_t num_events,
        void* event_paths,
        const FSEventStreamEventFlags*,
        const FSEventStreamEventId*
    ) {
        auto* watcher = static_cast<FileWatcher*>(info);
        auto** paths = static_cast<char**>(event_paths);
        auto filename = watcher->get_file_path().filename().string();

        for (size_t i = 0; i < num_events; ++i) {
            std::filesystem::path event_path(paths[i]);
            if (event_path.filename().string() == filename) {
                watcher->trigger_change();
                break;
            }
        }
    }
}

inline bool FileWatcher::start_native() {
    auto dir_path = file_path_.parent_path();
    if (dir_path.empty()) {
        dir_path = ".";
    }

    CFStringRef path_ref = CFStringCreateWithCString(
        kCFAllocatorDefault,
        dir_path.c_str(),
        kCFStringEncodingUTF8
    );

    CFArrayRef paths = CFArrayCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const void**>(&path_ref),
        1,
        &kCFTypeArrayCallBacks
    );

    FSEventStreamContext context{};
    context.info = this;

    impl_->stream = FSEventStreamCreate(
        kCFAllocatorDefault,
        &fsevents_callback_impl,
        &context,
        paths,
        kFSEventStreamEventIdSinceNow,
        0.1,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer
    );

    CFRelease(paths);
    CFRelease(path_ref);

    if (!impl_->stream) {
        return start_polling();
    }

    impl_->queue = dispatch_queue_create("com.livetuner.filewatcher", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(impl_->stream, impl_->queue);
    FSEventStreamStart(impl_->stream);

    return true;
}

inline void FileWatcher::stop_native() {
    if (impl_->stream) {
        FSEventStreamStop(impl_->stream);
        FSEventStreamInvalidate(impl_->stream);
        FSEventStreamRelease(impl_->stream);
        impl_->stream = nullptr;
    }
    if (impl_->queue) {
        dispatch_release(impl_->queue);
        impl_->queue = nullptr;
    }
}

#else
// Fallback: polling only
inline bool FileWatcher::start_native() {
    return start_polling();
}

inline void FileWatcher::stop_native() {
    // Nothing to do for polling mode
}
#endif

} // namespace internal

// ============================================================
// Params, LiveTuner, NlohmannParams - Destructor and Move Operations
// ============================================================
// These must be defined here because they use unique_ptr<FileWatcher>
// which requires complete type of FileWatcher::Impl

inline Params::~Params() {
    stop_watching();
}

inline Params::Params(Params&& other) noexcept
    : mtx_()
    , file_path_(std::move(other.file_path_))
    , format_(other.format_)
    , file_cache_(std::move(other.file_cache_))
    , bindings_(std::move(other.bindings_))
    , current_values_(std::move(other.current_values_))
    , file_watcher_(std::move(other.file_watcher_))
    , file_watcher_config_(std::move(other.file_watcher_config_))
    , file_read_retry_config_(std::move(other.file_read_retry_config_))
    , use_event_driven_(other.use_event_driven_)
    , file_changed_(other.file_changed_.load())
    , last_error_(std::move(other.last_error_))
    , on_change_callback_(std::move(other.on_change_callback_))
    , in_callback_(other.in_callback_.load())
{
}

inline Params& Params::operator=(Params&& other) noexcept {
    if (this != &other) {
        stop_watching();
        std::lock_guard<std::mutex> lock(mtx_);
        file_path_ = std::move(other.file_path_);
        format_ = other.format_;
        file_cache_ = std::move(other.file_cache_);
        bindings_ = std::move(other.bindings_);
        current_values_ = std::move(other.current_values_);
        file_watcher_ = std::move(other.file_watcher_);
        file_watcher_config_ = std::move(other.file_watcher_config_);
        file_read_retry_config_ = std::move(other.file_read_retry_config_);
        use_event_driven_ = other.use_event_driven_;
        file_changed_.store(other.file_changed_.load());
        last_error_ = std::move(other.last_error_);
        on_change_callback_ = std::move(other.on_change_callback_);
        in_callback_.store(other.in_callback_.load());
    }
    return *this;
}

inline LiveTuner::~LiveTuner() {
    if (file_watcher_) {
        file_watcher_->stop();
    }
}

inline LiveTuner::LiveTuner(LiveTuner&& other) noexcept
    : mtx_()
    , input_file_path_(std::move(other.input_file_path_))
    , file_cache_(std::move(other.file_cache_))
    , file_watcher_(std::move(other.file_watcher_))
    , file_watcher_config_(std::move(other.file_watcher_config_))
    , file_read_retry_config_(std::move(other.file_read_retry_config_))
    , use_event_driven_(other.use_event_driven_)
    , last_error_(std::move(other.last_error_))
{
}

inline LiveTuner& LiveTuner::operator=(LiveTuner&& other) noexcept {
    if (this != &other) {
        if (file_watcher_) {
            file_watcher_->stop();
        }
        std::lock_guard<std::mutex> lock(mtx_);
        input_file_path_ = std::move(other.input_file_path_);
        file_cache_ = std::move(other.file_cache_);
        file_watcher_ = std::move(other.file_watcher_);
        file_watcher_config_ = std::move(other.file_watcher_config_);
        file_read_retry_config_ = std::move(other.file_read_retry_config_);
        use_event_driven_ = other.use_event_driven_;
        last_error_ = std::move(other.last_error_);
    }
    return *this;
}

#ifdef LIVETUNER_USE_NLOHMANN_JSON
inline NlohmannParams::~NlohmannParams() {
    if (watcher_) {
        watcher_->stop();
    }
}
#endif // LIVETUNER_USE_NLOHMANN_JSON

} // namespace livetuner

// Restore MSVC warning level
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // LIVETUNER_IMPLEMENTATION