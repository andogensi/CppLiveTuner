#pragma once
/*
================================================================================
    LiveTuner.h - Live Parameter Tuning Library (STB-Style Single-Header)
================================================================================

    USAGE:
      In ONE .cpp file:    #define LIVETUNER_IMPLEMENTATION
                            #include "LiveTuner.h"
      
      In other files:       #include "LiveTuner.h"

================================================================================
*/

/**
 * @file LiveTuner.h
 * 
 * Live Parameter Tuning Library (STB-style single-header)
 * 
 * Inject values into running programs by modifying text files.
 * 
 * Features:
 * - Event-driven file watching (Windows/Linux/macOS)
 * - Non-blocking API for game loops
 * - JSON/YAML/INI support
 * - C++17, header-only
 * 
 * Includes picojson (BSD 2-Clause License) by K. Oku & Cybozu Labs.
 * For external picojson: #define LIVETUNER_USE_EXTERNAL_PICOJSON
 * 
 * @code
 * // Simple usage
 * float speed = 1.0f;
 * while (running) {
 *     tune_try(speed);
 *     player.move(speed);
 * }
 * 
 * // Named parameters
 * livetuner::Params params("config.json");
 * params.bind("speed", speed, 1.0f);
 * while (running) {
 *     params.update();
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
#include <cstdint>
#include <cinttypes>
#include <limits>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <array>
#include <iomanip>
#include <type_traits>
// ============================================================
// Configuration Macros
// ============================================================

// Set consistently in every translation unit to disable runtime tuning in a
// production build. Existing applications retain opt-in live tuning by default.
#ifndef LIVETUNER_ENABLE_RUNTIME
#define LIVETUNER_ENABLE_RUNTIME 1
#endif

// Development reads follow configured links by default. Hardened deployments
// can reject final symlinks/reparse points; define consistently in every TU.
// Explicit writes always reject links, independently of this read-only policy.
#ifndef LIVETUNER_STRICT_FILE_READS
#define LIVETUNER_STRICT_FILE_READS 0
#endif

/**
 * @def LIVETUNER_ENABLE_DEFAULT_LOGGING
 * @brief Enable/disable default logging to stderr
 * 
 * Default: 1 for debug builds, 0 for release builds.
 * Define as 0 before including to disable default logging.
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
// Platform headers are only included in LIVETUNER_IMPLEMENTATION section.
// FileWatcher uses PIMPL idiom to hide platform details.

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
    Unknown,               ///< Unknown error
    Disabled               ///< Runtime tuning disabled by build policy
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
            case ErrorType::Disabled: return "Disabled";
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
namespace internal {
    inline std::mutex& log_mutex() { static std::mutex mutex; return mutex; }
    inline LogCallback& log_callback_storage() {
        static LogCallback callback = default_log_handler;
        return callback;
    }
}

inline LogCallback get_global_log_callback() {
    std::lock_guard<std::mutex> lock(internal::log_mutex());
    return internal::log_callback_storage();
}

/**
 * @brief Set global log callback
 * 
 * By default, outputs to std::cerr in debug builds.
 * Set custom callback or nullptr to disable.
 * 
 * @code
 * livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
 *     MyLogger::log(level, msg);
 * });
 * @endcode
 */
inline void set_log_callback(LogCallback callback) {
    // Destroy the previous callback after unlocking (its captures may reenter).
    {
        std::lock_guard<std::mutex> lock(internal::log_mutex());
        internal::log_callback_storage().swap(callback);
    }
}

// ============================================================
// Vendored: picojson
// ============================================================
// picojson - Copyright 2009-2010 Cybozu Labs, 2011-2014 Kazuho Oku
// 2-clause BSD License (see LICENSE section at end of file)
//
// To use external picojson: #define LIVETUNER_USE_EXTERNAL_PICOJSON

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
template <> inline const double& value::get<double>() const {
  if (!is<double>()) throw std::runtime_error("type mismatch");
  if (type_ == int64_type) {
    auto* self = const_cast<value*>(this);
    self->u_.number_ = static_cast<double>(u_.int64_);
    self->type_ = number_type;
  }
  return u_.number_;
}
template <> inline double& value::get<double>() {
  return const_cast<double&>(static_cast<const value&>(*this).get<double>());
}
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
      errno = 0;
      f = strtod(num_str.c_str(), &endp);
      if (errno != ERANGE && std::isfinite(f) && endp == num_str.c_str() + num_str.size()) {
        return ctx.set_number(f);
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
    --depths_;
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
  auto end = parse(out, s.begin(), s.end(), &err);
  while (end != s.end() && std::isspace(static_cast<unsigned char>(*end))) ++end;
  if (err.empty() && end != s.end()) err = "trailing data after JSON value";
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

// Shared budgets apply before allocating parser trees, including optional backends.
inline constexpr size_t max_config_bytes = 1024 * 1024;
inline constexpr size_t max_config_entries = 4096;
inline constexpr size_t max_config_depth = 64;
inline constexpr size_t max_key_bytes = 1024;
inline constexpr size_t max_value_bytes = 64 * 1024;

inline bool json_within_limits(const std::string& content) {
    if (content.size() > max_config_bytes) return false;
    size_t depth = 0, entries = 0, string_bytes = 0;
    bool quoted = false, escaped = false;
    for (unsigned char c : content) {
        if (quoted) {
            if (++string_bytes > max_value_bytes) return false;
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') {
            quoted = true;
            string_bytes = 0;
        } else if (c == '{' || c == '[') {
            if (++depth > max_config_depth || ++entries > max_config_entries) return false;
        } else if (c == '}' || c == ']') {
            if (depth == 0) return false;
            --depth;
        } else if (c == ',') {
            if (++entries > max_config_entries) return false;
        }
    }
    return !quoted && depth == 0;
}

inline std::string safe_log_message(const std::string& input) {
    std::string output;
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : input) {
        if (output.size() >= 2048) { output += " [truncated]"; break; }
        if (c < 0x20 || c == 0x7f || c == 0x1b) {
            output += "\\x";
            output += hex[c >> 4];
            output += hex[c & 15];
        } else output += static_cast<char>(c);
    }
    return output;
}

// Declare before acquiring instance locks. Nested scopes flush only at the
// outer boundary, after all locks have been released.
class DeferredCallbacks {
    inline static thread_local DeferredCallbacks* active_ = nullptr;
    DeferredCallbacks* previous_;
    std::vector<std::function<void()>> pending_;
public:
    DeferredCallbacks() : previous_(active_) { active_ = this; }
    DeferredCallbacks(const DeferredCallbacks&) = delete;
    DeferredCallbacks& operator=(const DeferredCallbacks&) = delete;
    ~DeferredCallbacks() noexcept {
        active_ = previous_;
        for (auto& callback : pending_) {
            try { dispatch(std::move(callback)); }
            catch (...) { default_log_handler(LogLevel::Error, "Diagnostic callback threw an exception"); }
        }
    }
    static void dispatch(std::function<void()> callback) {
        if (active_) active_->pending_.push_back(std::move(callback));
        else callback();
    }
};

/**
 * @brief Log output helper
 */
inline void log(LogLevel level, const std::string& message) {
    auto callback = get_global_log_callback();
    if (!callback) return;
    DeferredCallbacks::dispatch([callback = std::move(callback), level, text = safe_log_message(message)] {
        static thread_local bool delivering = false;
        if (delivering) return; // A logger may query the API, but must not recursively log.
        delivering = true;
        try { callback(level, text); }
        catch (...) { delivering = false; throw; }
        delivering = false;
    });
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

// Native-handle reads always require a regular file and bound actual bytes.
// Final links are followed unless LIVETUNER_STRICT_FILE_READS is enabled.
FileReadResult read_regular_file(const std::filesystem::path& path);
bool write_regular_file(const std::filesystem::path& path, const std::string& content, ErrorInfo& error);

// Polling APIs perform one bounded read. They never wait for an editor to finish;
// a later update/try_get call retries, and the caller controls diagnostic delivery.
inline std::optional<std::string> read_file_once(const std::filesystem::path& path, ErrorInfo& error) {
    try {
        auto result = read_regular_file(path);
        error = std::move(result.error);
        return std::move(result.content);
    } catch (const std::exception& e) {
        error = ErrorInfo(ErrorType::FileReadError, e.what(), path.string());
    } catch (...) {
        error = ErrorInfo(ErrorType::FileReadError, "Unknown exception during file read", path.string());
    }
    return std::nullopt;
}

inline void log_changed_error(const ErrorInfo& error, const ErrorInfo& previous) {
    if (error && (error.type != previous.type || error.message != previous.message ||
                  error.file_path != previous.file_path))
        log(LogLevel::Warning, error.to_string());
}

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
    ErrorInfo* error_out = nullptr,
    const std::atomic<bool>* cancelled = nullptr)
{
    // Invalid caller retry policies must not turn shutdown into an unbounded wait.
    if (config.max_retries < 0 || config.max_retries > 16 || config.retry_delay.count() < 0 ||
        config.retry_delay > std::chrono::seconds(1) || !std::isfinite(config.backoff_multiplier) ||
        config.backoff_multiplier < 1.0 || config.backoff_multiplier > 4.0) {
        if (error_out) *error_out = ErrorInfo(ErrorType::InvalidFormat, "Invalid file retry policy", path.string());
        return std::nullopt;
    }
    auto delay = config.retry_delay;
    ErrorInfo last_error;
    std::string path_str = path.string();
    
    for (int attempt = 0; attempt <= config.max_retries; ++attempt) {
        if (cancelled && cancelled->load()) return std::nullopt;
        if (attempt > 0) {
            auto remaining = delay;
            while (remaining.count() > 0) {
                if (cancelled && cancelled->load()) return std::nullopt;
                const auto step = std::min(remaining, std::chrono::milliseconds(20));
                std::this_thread::sleep_for(step);
                remaining -= step;
            }
            delay = std::chrono::milliseconds(
                static_cast<int>(std::min(1000.0, delay.count() * config.backoff_multiplier)));
        }
        
        try {
            auto result = read_regular_file(path);
            if (result) {
                if (error_out) *error_out = ErrorInfo{};
                return std::move(result.content);
            }
            last_error = std::move(result.error);
            if (last_error.type == ErrorType::InvalidFormat) break;
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
 * @brief Cross-platform file watcher (PIMPL pattern)
 * 
 * Uses OS native APIs: Windows (ReadDirectoryChangesW), Linux (inotify), macOS (FSEvents).
 * Requires LIVETUNER_IMPLEMENTATION in one source file. Uses polling when native setup fails.
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void()>;

    FileWatcher();
    explicit FileWatcher(const FileWatcherConfig& config);
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

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
    void signal_native();
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
    Json,       ///< JSON format (full support via picojson)
    Yaml        ///< YAML format (simple key: value only, no nesting/arrays)
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
            if constexpr (std::is_floating_point_v<T>) {
                if (!std::isfinite(temp)) return false;
            }
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
 * Extracts values from JSON objects, including nested structures
 * Nested keys are flattened using dot notation (e.g., "parent.child.value")
 */
class PicojsonParser {
public:
    using ValueMap = std::unordered_map<std::string, std::string>;
    
    static bool parse(const std::string& content, ValueMap& result) {
        result.clear();
        if (!json_within_limits(content)) return false;
        try {
            picojson::value v;
            std::string err;
            auto end = picojson::parse(v, content.begin(), content.end(), &err);
            while (end != content.end() && std::isspace(static_cast<unsigned char>(*end))) ++end;
            if (!err.empty() || end != content.end() || !v.is<picojson::object>()) return false;
            ValueMap candidate;
            flatten_object(v.get<picojson::object>(), "", candidate);
            result = std::move(candidate);
            return true;
        } catch (const std::exception&) {
            result.clear();
            return false;
        }
    }

private:
    /**
     * @brief Convert double to string, outputting integers without decimal point
     * 
     * Examples:
     *   97.0 -> "97"
     *   3.14 -> "3.14"
     *   100.500 -> "100.5"
     */
    static std::string double_to_string(double value) {
        // Check if the value is an integer
        if (std::floor(value) == value && std::abs(value) < 1e15) {
            // Output as integer (without decimal point)
            return std::to_string(static_cast<long long>(value));
        }
        
        // For floating-point values, format properly
        std::ostringstream oss;
        oss << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        std::string str = oss.str();
        
        // Preserve small finite values and scientific notation without rounding.
        return str;
    }
    
    /**
     * @brief Recursively flatten nested JSON objects into dot-notation keys
     * 
     * @param obj The picojson object to flatten
     * @param prefix The current key prefix (empty for root level)
     * @param result The output map to store flattened key-value pairs
     */
    static void flatten_object(const picojson::object& obj, const std::string& prefix, ValueMap& result) {
        for (const auto& kv : obj) {
            std::string key = prefix.empty() ? kv.first : prefix + "." + kv.first;
            const picojson::value& val = kv.second;
            if (key.size() > max_key_bytes || result.size() >= max_config_entries ||
                (val.is<std::string>() && val.get<std::string>().size() > max_value_bytes))
                throw std::length_error("configuration entry exceeds resource limits");
            
            if (val.is<std::string>()) {
                result[key] = val.get<std::string>();
#ifdef PICOJSON_USE_INT64
            } else if (val.is<int64_t>()) {
                result[key] = std::to_string(val.get<int64_t>());
#endif
            } else if (val.is<double>()) {
                result[key] = double_to_string(val.get<double>());
            } else if (val.is<bool>()) {
                result[key] = val.get<bool>() ? "true" : "false";
            } else if (val.is<picojson::null>()) {
                result[key] = "";
            } else if (val.is<picojson::object>()) {
                // Recursively flatten nested objects
                flatten_object(val.get<picojson::object>(), key, result);
            } else if (val.is<picojson::array>()) {
                // Flatten arrays with index notation (e.g., "key.0", "key.1")
                flatten_array(val.get<picojson::array>(), key, result);
            }
        }
    }
    
    /**
     * @brief Recursively flatten JSON arrays into indexed dot-notation keys
     * 
     * @param arr The picojson array to flatten
     * @param prefix The current key prefix
     * @param result The output map to store flattened key-value pairs
     */
    static void flatten_array(const picojson::array& arr, const std::string& prefix, ValueMap& result) {
        for (size_t i = 0; i < arr.size(); ++i) {
            std::string key = prefix + "." + std::to_string(i);
            const picojson::value& val = arr[i];
            if (key.size() > max_key_bytes || result.size() >= max_config_entries ||
                (val.is<std::string>() && val.get<std::string>().size() > max_value_bytes))
                throw std::length_error("configuration entry exceeds resource limits");
            
            if (val.is<std::string>()) {
                result[key] = val.get<std::string>();
#ifdef PICOJSON_USE_INT64
            } else if (val.is<int64_t>()) {
                result[key] = std::to_string(val.get<int64_t>());
#endif
            } else if (val.is<double>()) {
                result[key] = double_to_string(val.get<double>());
            } else if (val.is<bool>()) {
                result[key] = val.get<bool>() ? "true" : "false";
            } else if (val.is<picojson::null>()) {
                result[key] = "";
            } else if (val.is<picojson::object>()) {
                flatten_object(val.get<picojson::object>(), key, result);
            } else if (val.is<picojson::array>()) {
                flatten_array(val.get<picojson::array>(), key, result);
            }
        }
    }
};

/**
 * @brief Lightweight YAML/INI parser (no external dependencies)
 * 
 * Simple key-value parser, NOT a full YAML parser.
 * Supports: key:value, key=value, comments (#;), quoted values.
 * NOT supported: nested structures, arrays, multi-line strings, anchors, tags.
 * For full YAML support, use yaml-cpp or convert to JSON.
 */
class SimpleKeyValueParser {
public:
    using ValueMap = std::unordered_map<std::string, std::string>;
    
    static bool parse(const std::string& content, ValueMap& result, bool yaml_style = false) {
        result.clear();
        if (content.size() > max_config_bytes) return false;
        try {
        ValueMap candidate;
        size_t entries = 0;
        
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
                    if (++entries > max_config_entries || key.size() > max_key_bytes ||
                        value.size() > max_value_bytes) return false;
                    candidate[key] = value;
                } else return false;
            } else return false;
        }
        result = std::move(candidate);
        return true;
        } catch (const std::exception&) {
            result.clear();
            return false;
        }
    }
};

} // namespace internal

// ============================================================
// FileWatcherConfig Export
// ============================================================

/**
 * @brief File watcher configuration
 * 
 * Configure buffer size and overflow handling for file watching.
 * @see Params::set_watcher_config()
 */
using FileWatcherConfig = internal::FileWatcherConfig;

/**
 * @brief File read retry configuration
 * 
 * Prevents read errors while editor is writing (especially on Windows).
 * @see Params::set_read_retry_config()
 */
using FileReadRetryConfig = internal::FileReadRetryConfig;

// ============================================================
// Params Class (Named Parameters)
// ============================================================

/**
 * @brief Named parameter management
 * 
 * Binds multiple variables to a file and updates in batch.
 * 
 * @note Thread Safety: Callbacks run on the thread calling update() (main thread).
 * Safe for OpenGL/DirectX. Differs from LiveTuner's background thread callbacks.
 */
class Params {
public:
    struct FileCache {
        std::filesystem::file_time_type last_modify_time;
        std::chrono::steady_clock::time_point last_access;
        bool file_exists = false;
        bool attempted = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };

private:
    // Base class for holding binding information
    struct BindingBase {
        virtual ~BindingBase() = default;
        virtual bool prepare(const std::string* str_value) = 0;
        virtual void commit() = 0;
        virtual void apply_default() = 0;
    };
    
    template<typename T>
    struct Binding : public BindingBase {
        T* target;
        T default_value;
        std::optional<T> prepared;
        
        Binding(T* t, T def) : target(t), default_value(def) {}
        
        bool prepare(const std::string* str_value) override {
            T value = default_value;
            if (str_value && !internal::parse_value(*str_value, value)) return false;
            prepared = std::move(value);
            return true;
        }
        void commit() override { *target = std::move(*prepared); prepared.reset(); }
        
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
    std::shared_ptr<std::atomic<bool>> file_changed_ = std::make_shared<std::atomic<bool>>(false);
    
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
     * @note Retained for compatibility. update()/poll() always make one attempt.
     */
    internal::FileReadRetryConfig get_read_retry_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_read_retry_config_;
    }
    
    /**
     * @brief Set file read retry configuration
     * 
     * Avoids conflicts while editor is writing (especially on Windows).
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
    internal::FileWatcherConfig get_watcher_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_watcher_config_;
    }
    
    /**
     * @brief Set file watcher configuration
     * 
     * Changes take effect on next start_watching() call.
     */
    void set_watcher_config(const internal::FileWatcherConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_watcher_config_ = config;
        file_watcher_config_.validate();
    }

    /**
     * @brief Bind variable to parameter
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
        internal::DeferredCallbacks diagnostics;
        if (!LIVETUNER_ENABLE_RUNTIME) return false;
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
            
            
            auto now = std::chrono::steady_clock::now();
            auto current_modify_time = internal::get_file_modify_time(file_path_);
            
            // Successful and failed probes expire: mtime is only a hint, not a
            // content identity (editors/filesystems can preserve or coarsen it).
            if (file_cache_.attempted &&
                now - file_cache_.last_access < FileCache::cache_duration &&
                current_modify_time == file_cache_.last_modify_time) {
                return false;
            }
            
            const auto previous_error = last_error_;
            updated = load_file();
            internal::log_changed_error(last_error_, previous_error);
            
            file_cache_.last_modify_time = current_modify_time;
            file_cache_.last_access = now;
            file_cache_.file_exists = !last_error_;
            file_cache_.attempted = true;
            
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
     * Monitors file in background and marks file as changed; call poll() from main loop on changes.
     */
    void start_watching() {
        if (!LIVETUNER_ENABLE_RUNTIME) return;
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
        if (!file_changed_) file_changed_ = std::make_shared<std::atomic<bool>>(false);
        file_changed_->store(true); // Initial read
        if (!file_watcher_->start(file_path_, [changed = file_changed_] {
            changed->store(true);
        })) last_error_ = ErrorInfo(ErrorType::WatcherError, "Cannot start file watcher", file_path_);
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
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!file_watcher_) return false;
            if (file_changed_ && file_changed_->exchange(false)) {
                file_cache_.file_exists = false;
                file_cache_.attempted = false;
            }
        }
        // Also probe periodically: a symlink target may be in a different watched
        // directory, and a notification may arrive before an editor finishes.
        return update();
    }

    /**
     * @brief Set callback for changes
     * 
     * @note Thread Safety: Callback runs on the thread calling update() (main thread).
     * Safe for OpenGL/DirectX/UI operations.
     */
    void on_change(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(mtx_);
        on_change_callback_ = std::move(callback);
    }

    /**
     * @brief Get last error information
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
        invalidate_cache_locked();
        
        // If watching, restart
        if (file_watcher_ && file_watcher_->is_running()) {
            file_watcher_->stop();
            file_watcher_->start(file_path_, [changed = file_changed_] {
                changed->store(true);
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
        
        std::lock_guard<std::mutex> lock(mtx_);
        invalidate_cache_locked();
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
    void invalidate_cache_locked() {
        file_cache_ = FileCache{};
        current_values_.clear();
    }

    bool load_file() {
        // update() is a polling API: never use the sleeping retry helper here.
        ErrorInfo read_error;
        auto content_opt = internal::read_file_once(file_path_, read_error);
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
            if (!parsed) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse JSON format", file_path_);
            }
            break;
        case FileFormat::Yaml:
            parsed = internal::SimpleKeyValueParser::parse(content, new_values, true);
            if (!parsed) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse YAML format", file_path_);
            }
            break;
        case FileFormat::KeyValue:
        case FileFormat::Plain:
        default:
            parsed = internal::SimpleKeyValueParser::parse(content, new_values, false);
            if (!parsed) {
                last_error_ = ErrorInfo(ErrorType::ParseError, 
                                       "Failed to parse key-value format", file_path_);
            }
            break;
        }
        
        if (!parsed) {
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
            last_error_ = {};
            return false;
        }
        
        // Validate every bound value before publishing any of the new snapshot.
        for (auto& [name, binding] : bindings_) {
            auto it = new_values.find(name);
            if (!binding->prepare(it == new_values.end() ? nullptr : &it->second)) {
                last_error_ = ErrorInfo(ErrorType::ParseError,
                    "Invalid value for parameter '" + name + "'", file_path_);
                return false;
            }
        }
        for (auto& [name, binding] : bindings_) binding->commit();
        current_values_ = std::move(new_values);

        // Clear error on success
        last_error_ = ErrorInfo();
        
        return true;
    }
};

// ============================================================
// LiveTuner Class
// ============================================================

/**
 * @brief Exception delivered by a future when its owner cancels the operation
 */
class AsyncCancelled : public std::runtime_error {
public:
    AsyncCancelled() : std::runtime_error("LiveTuner asynchronous read cancelled") {}
};

/**
 * @brief Live parameter tuning (low-level, single-value API)
 *
 * try_get() performs one read without retry sleeps; get()/get_timeout() and
 * get_async() wait for valid input using the configured retry policy.
 * Pending asynchronous work moves with its owner and is cancelled on reset or
 * destruction. Configuration paths are never created by read operations.
 *
 * @warning Asynchronous callbacks execute on a worker thread, not the caller's
 * thread. Use Params for main-thread callbacks such as OpenGL/DirectX updates.
 * Do not race object destruction/move with external member calls.
 */
class LiveTuner {
public:
    struct FileCache {
        std::filesystem::file_time_type last_modify_time{};
        std::chrono::steady_clock::time_point last_access{};
        bool file_exists = false;
        bool attempted = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };
private:
    struct ReadConfig {
        std::string path;
        internal::FileWatcherConfig watcher;
        internal::FileReadRetryConfig retry;
        bool event_driven;
    };
    struct AsyncState {
        std::atomic<bool> cancelled{false};
        std::atomic<bool> done{false};
    };
    struct AsyncTask {
        std::shared_ptr<AsyncState> state = std::make_shared<AsyncState>();
        std::thread worker;
    };
    mutable std::mutex mtx_;
    std::string input_file_path_ = "params.txt";
    FileCache file_cache_{};
    internal::FileWatcherConfig file_watcher_config_;
    internal::FileReadRetryConfig file_read_retry_config_;
    bool use_event_driven_ = true;
    ErrorInfo last_error_;
    std::vector<std::unique_ptr<AsyncTask>> async_tasks_;

    ReadConfig read_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return {input_file_path_, file_watcher_config_, file_read_retry_config_, use_event_driven_};
    }

    template<typename T>
    static bool read_value(const ReadConfig& config, T& value, ErrorInfo& error,
                           const std::atomic<bool>* cancelled = nullptr, bool allow_retry = true) {
        auto content = allow_retry
            ? internal::read_file_with_retry(config.path, config.retry, &error, cancelled)
            : internal::read_file_once(config.path, error);
        if (!content) return false;
        try {
            std::istringstream stream(*content);
            std::string line;
            while (std::getline(stream, line)) {
                line = internal::trim(line);
                if (line.empty() || line[0] == '#') continue;
                if (line.size() > internal::max_value_bytes) break;
                T candidate{};
                if (internal::parse_value(line, candidate)) {
                    value = std::move(candidate);
                    error = {};
                    return true;
                }
            }
        } catch (const std::exception&) {
            // User-defined extraction can fail; keep the previously accepted value.
        }
        error = ErrorInfo(ErrorType::ParseError, "No valid value found in file", config.path);
        return false;
    }

    template<typename T>
    static bool wait_value(const ReadConfig& config, T& value,
                           std::optional<std::chrono::milliseconds> timeout,
                           const std::shared_ptr<AsyncState>& state, ErrorInfo& error) {
        if (!LIVETUNER_ENABLE_RUNTIME) {
            error = ErrorInfo(ErrorType::Disabled, "Runtime tuning is disabled", config.path);
            return false;
        }
        internal::FileWatcher watcher(config.watcher);
        const bool watching = config.event_driven && watcher.start(config.path, [] {});
        const auto started = std::chrono::steady_clock::now();
        for (;;) {
            if (state && state->cancelled.load()) return false;
            if (read_value(config, value, error, state ? &state->cancelled : nullptr)) return true;
            auto interval = std::chrono::milliseconds(20);
            if (timeout) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started);
                if (elapsed >= *timeout) {
                    error = ErrorInfo(ErrorType::Timeout, "Timeout waiting for valid value", config.path);
                    return false;
                }
                interval = std::min(interval, *timeout - elapsed);
            }
            if (watching) watcher.wait_for_change(interval);
            else std::this_thread::sleep_for(interval);
        }
    }

    template<typename Work>
    void start_async(Work work) {
        // Keep an outer owner until the lock is released, including thread-creation
        // failure. Destructors of callback captures can execute application code.
        auto operation = std::make_shared<Work>(std::move(work));
        std::lock_guard<std::mutex> lock(mtx_);
        // Completed tasks no longer execute user code and can be reaped under lock.
        for (auto it = async_tasks_.begin(); it != async_tasks_.end();) {
            if ((*it)->state->done.load()) {
                (*it)->worker.join();
                it = async_tasks_.erase(it);
            } else ++it;
        }
        auto task = std::make_unique<AsyncTask>();
        auto state = task->state;
        async_tasks_.push_back(std::move(task));
        try {
            async_tasks_.back()->worker = std::thread([state, operation]() mutable {
                try { (*operation)(state); }
                catch (...) {
                    try { internal::log(LogLevel::Error, "Asynchronous callback threw an exception"); }
                    catch (...) { internal::default_log_handler(LogLevel::Error, "Asynchronous callback and logger failed"); }
                }
                operation.reset();
                state->done.store(true);
            });
        } catch (...) {
            async_tasks_.pop_back();
            throw;
        }
    }

    void stop_async() {
        std::vector<std::unique_ptr<AsyncTask>> tasks;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks.swap(async_tasks_);
        }
        for (auto& task : tasks) task->state->cancelled.store(true);
        for (auto& task : tasks) {
            if (!task->worker.joinable()) continue;
            if (task->worker.get_id() == std::this_thread::get_id()) {
                // A callback may destroy its owner. This worker owns only copied
                // config/state and callback captures; it never accesses the owner.
                task->worker.detach();
            } else task->worker.join();
        }
    }

    void invalidate_cache_locked() { file_cache_ = FileCache{}; }

public:
    /**
     * @brief Construct a tuner for params.txt in the process working directory
     */
    LiveTuner() = default;

    /**
     * @brief Construct a tuner for an existing file, or one supplied later
     */
    explicit LiveTuner(std::string_view path) : input_file_path_(path) {}

    LiveTuner(const LiveTuner&) = delete;
    LiveTuner& operator=(const LiveTuner&) = delete;
    LiveTuner(LiveTuner&&) noexcept;
    LiveTuner& operator=(LiveTuner&&) noexcept;
    ~LiveTuner();

    /**
     * @brief Get a snapshot of the blocking/asynchronous read retry policy
     */
    internal::FileReadRetryConfig get_read_retry_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_read_retry_config_;
    }

    /**
     * @brief Set retries for future waiting operations; try_get() never sleeps
     */
    void set_read_retry_config(const internal::FileReadRetryConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_read_retry_config_ = config;
    }

    /**
     * @brief Get a snapshot of the file watcher configuration
     */
    internal::FileWatcherConfig get_watcher_config() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return file_watcher_config_;
    }

    /**
     * @brief Configure watchers created by subsequent waiting operations
     */
    void set_watcher_config(const internal::FileWatcherConfig& config) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_watcher_config_ = config;
        file_watcher_config_.validate();
    }

    /**
     * @brief Change the file for subsequent reads and invalidate the polling cache
     * @note Already started asynchronous operations retain their path snapshot.
     */
    void set_file(std::string_view path) {
        std::lock_guard<std::mutex> lock(mtx_);
        input_file_path_ = path;
        invalidate_cache_locked();
    }

    /**
     * @brief Get a snapshot of the configured path
     */
    std::string get_file() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return input_file_path_;
    }

    /**
     * @brief Enable native notifications for subsequent waiting operations
     */
    void set_event_driven(bool enabled) {
        std::lock_guard<std::mutex> lock(mtx_);
        use_event_driven_ = enabled;
    }

    /**
     * @brief Check whether subsequent waits will use native notifications
     */
    bool is_event_driven() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return use_event_driven_;
    }

    /**
     * @brief Check whether this platform has a native file-watching backend
     */
    static bool has_native_file_watch() {
        return internal::FileWatcher::has_native_support();
    }

    /**
     * @brief Get the last synchronous-read error; async futures report their own errors
     */
    ErrorInfo last_error() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return last_error_;
    }

    /**
     * @brief Check for a synchronous-read error
     */
    bool has_error() const {
        return static_cast<bool>(last_error());
    }

    /**
     * @brief Clear the stored synchronous-read error
     */
    void clear_error() {
        std::lock_guard<std::mutex> lock(mtx_);
        last_error_ = {};
    }

    /**
     * @brief Try one bounded read without sleeping or creating a file
     *
     * Matching timestamps are cached for at most 10 ms, including failed probes.
     * After expiry, contents are read again even if mtime did not change. A
     * successful re-read can therefore return true for an unchanged value.
     * Identical consecutive errors are logged once and remain in last_error().
     *
     * @param value Destination, left unchanged on failure
     * @return true on a successful read; false for a cache hit or an error
     * @note Serialized by the instance mutex. OS/file I/O is synchronous, so this
     * is not a hard real-time guarantee for slow/network/cloud filesystems.
     */
    template<typename T>
    bool try_get(T& value) {
        internal::DeferredCallbacks diagnostics;
        std::lock_guard<std::mutex> lock(mtx_);
        if (!LIVETUNER_ENABLE_RUNTIME) {
            last_error_ = ErrorInfo(ErrorType::Disabled, "Runtime tuning is disabled", input_file_path_);
            return false;
        }
        const auto modified = internal::get_file_modify_time(input_file_path_);
        const auto now = std::chrono::steady_clock::now();
        if (file_cache_.attempted && now - file_cache_.last_access < FileCache::cache_duration &&
            modified == file_cache_.last_modify_time) return false;
        const ReadConfig config{input_file_path_, file_watcher_config_, file_read_retry_config_, use_event_driven_};
        const auto previous_error = last_error_;
        const bool read = read_value(config, value, last_error_, nullptr, false);
        file_cache_ = {modified, now, read, true};
        internal::log_changed_error(last_error_, previous_error);
        return read;
    }

    /**
     * @brief Wait for a valid value, leaving the destination unchanged on error
     */
    template<typename T>
    void get(T& value) {
        const auto config = read_config();
        ErrorInfo error;
        wait_value(config, value, std::nullopt, nullptr, error);
        std::lock_guard<std::mutex> lock(mtx_);
        last_error_ = std::move(error);
    }

    /**
     * @brief Wait for a valid value with a timeout; return false on timeout/error
     */
    template<typename T>
    bool get_timeout(T& value, std::chrono::milliseconds timeout) {
        const auto config = read_config();
        ErrorInfo error;
        const bool read = wait_value(config, value, timeout, nullptr, error);
        std::lock_guard<std::mutex> lock(mtx_);
        last_error_ = std::move(error);
        return read;
    }

    /**
     * @brief Start an owned asynchronous read with a path/settings snapshot
     * @return Future containing a value or an exception (AsyncCancelled on cancel)
     */
    template<typename T>
    std::future<T> get_async() {
        auto promise = std::make_shared<std::promise<T>>();
        auto future = promise->get_future();
        start_async([config = read_config(), promise](const auto& state) {
            try {
                T value{};
                ErrorInfo error;
                if (!wait_value(config, value, std::nullopt, state, error) || state->cancelled.load()) {
                    if (error.type == ErrorType::Disabled) throw std::runtime_error(error.to_string());
                    throw AsyncCancelled{};
                }
                promise->set_value(std::move(value));
            } catch (...) { promise->set_exception(std::current_exception()); }
        });
        return future;
    }

    /**
     * @brief Read asynchronously and invoke the callback on the worker thread
     * @note Keep external callback captures alive until completion. Pending
     * callbacks are cancelled on reset/destruction; running callbacks are drained.
     */
    template<typename T, typename Callback>
    void get_async(Callback callback) {
        start_async([config = read_config(), callback = std::move(callback)](const auto& state) mutable {
            T value{};
            ErrorInfo error;
            if (wait_value(config, value, std::nullopt, state, error) && !state->cancelled.load())
                callback(std::move(value));
        });
    }

    /**
     * @brief Force a new probe on the next try_get() call
     */
    void invalidate_cache() {
        std::lock_guard<std::mutex> lock(mtx_);
        invalidate_cache_locked();
    }

    /**
     * @brief Cancel/drain pending operations and clear the polling/error state
     */
    void reset() {
        stop_async();
        std::lock_guard<std::mutex> lock(mtx_);
        invalidate_cache_locked();
        last_error_ = {};
    }
};

// ============================================================
// Dependency Injection Support (Testable Design)
// ============================================================

/**
 * @brief RAII context for LiveTuner
 * 
 * Avoids global state in tests and large applications.
 * Automatically cleans up when leaving scope.
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
 * @brief RAII context for Params
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
 */
inline LiveTuner& get_default_tuner() {
    return internal::GlobalInstanceManager::instance().get_tuner();
}

/**
 * @brief Get default Params instance
 */
inline Params& get_default_params() {
    return internal::GlobalInstanceManager::instance().get_params();
}

// ============================================================
// Convenient Global Functions (For Personal Development and Small Projects)
// ============================================================

/**
 * @brief Set file to monitor
 */
inline void tune_init(std::string_view file_path) {
    get_default_tuner().set_file(file_path);
}

/**
 * @brief Try to read value immediately (non-blocking)
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
 */
inline void params_init(std::string_view file_path, FileFormat format = FileFormat::Auto) {
    get_default_params().set_file(file_path, format);
}

/**
 * @brief Bind variable to global Params
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
 */
inline void reset_global_params() {
    internal::GlobalInstanceManager::instance().reset_params();
}

/**
 * @brief Reset all global instances
 */
inline void reset_all_globals() {
    internal::GlobalInstanceManager::instance().reset_all();
}
#endif // LIVETUNER_ENABLE_TEST_SUPPORT

// ============================================================
// nlohmann/json Support (Optional Feature)
// ============================================================

#ifdef LIVETUNER_USE_NLOHMANN_JSON

#ifndef NLOHMANN_JSON_VERSION_MAJOR
#error "LIVETUNER_USE_NLOHMANN_JSON is defined but nlohmann/json.hpp is not included. Please #include <nlohmann/json.hpp> before LiveTuner.h"
#endif

namespace internal {
// nlohmann's native arithmetic conversions can wrap or overflow without throwing.
// Check before conversion, including the standard containers used by this adapter.
template<typename T>
struct JsonNumericRange {
    static void validate(const nlohmann::json& value) {
        bool valid = true;
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (value.is_number_unsigned()) {
                valid = value.get<uint64_t>() <= static_cast<uint64_t>(std::numeric_limits<T>::max());
            } else if (value.is_number_integer()) {
                const auto number = value.get<int64_t>();
                if constexpr (std::is_signed_v<T>) {
                    valid = number >= static_cast<int64_t>(std::numeric_limits<T>::min()) &&
                            number <= static_cast<int64_t>(std::numeric_limits<T>::max());
                } else {
                    valid = number >= 0 && static_cast<uint64_t>(number) <=
                            static_cast<uint64_t>(std::numeric_limits<T>::max());
                }
            } else if (value.is_number_float()) {
                const double number = std::trunc(value.get<double>());
                // An exclusive power-of-two upper bound avoids rounding INT64_MAX
                // up to 2^63 on platforms where long double is also 64-bit.
                const double upper = std::ldexp(1.0, std::numeric_limits<T>::digits);
                valid = std::isfinite(number) && number < upper &&
                        number >= (std::is_signed_v<T> ? -upper : 0.0);
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            if (value.is_number()) {
                const double number = value.get<double>();
                valid = std::isfinite(number) && number <= std::numeric_limits<T>::max() &&
                        number >= -std::numeric_limits<T>::max();
            }
        }
        if (!valid) throw std::out_of_range("JSON number is outside the destination type range");
    }
};

template<typename T, typename Allocator>
struct JsonNumericRange<std::vector<T, Allocator>> {
    static void validate(const nlohmann::json& value) {
        if (value.is_array()) for (const auto& element : value) JsonNumericRange<T>::validate(element);
    }
};

template<typename T, size_t N>
struct JsonNumericRange<std::array<T, N>> {
    static void validate(const nlohmann::json& value) {
        if (value.is_array()) for (const auto& element : value) JsonNumericRange<T>::validate(element);
    }
};

template<typename T, typename Compare, typename Allocator>
struct JsonNumericRange<std::map<std::string, T, Compare, Allocator>> {
    static void validate(const nlohmann::json& value) {
        if (value.is_object()) for (const auto& element : value) JsonNumericRange<T>::validate(element);
    }
};

template<typename T, typename Hash, typename Equal, typename Allocator>
struct JsonNumericRange<std::unordered_map<std::string, T, Hash, Equal, Allocator>> {
    static void validate(const nlohmann::json& value) {
        if (value.is_object()) for (const auto& element : value) JsonNumericRange<T>::validate(element);
    }
};
} // namespace internal

/**
 * @brief Parameter management for nlohmann/json
 * 
 * Supports nested objects, arrays, and type-safe value retrieval.
 * Requires: #include <nlohmann/json.hpp> before this header.
 * 
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

        if (!LIVETUNER_ENABLE_RUNTIME) return;
        
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
        if (!LIVETUNER_ENABLE_RUNTIME) return false;
        return load(watcher_->wait_for_change(std::chrono::milliseconds(0)));
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
        internal::DeferredCallbacks diagnostics;
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            json value = get_value_by_path(json_, json_path);
            if (value.is_null()) {
                return default_value;
            }
            internal::JsonNumericRange<T>::validate(value);
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
        internal::DeferredCallbacks diagnostics;
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
        {
            std::lock_guard<std::mutex> lock(mutex_);
            error_callback_.swap(callback);
        }
    }

    /**
     * @brief Get last error
     * @return Error information
     */
    ErrorInfo last_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    /**
     * @brief Save current JSON to file
     * @param pretty Whether to format output
     * @return true if successful
     */
    bool save(bool pretty = true) const {
        internal::DeferredCallbacks diagnostics;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!LIVETUNER_ENABLE_RUNTIME) return false;
        
        try {
            ErrorInfo error;
            if (!internal::write_regular_file(file_path_, json_.dump(pretty ? 2 : -1), error)) {
                handle_error(error.type, error.message);
                return false;
            }
            last_error_ = {};
            
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
        internal::DeferredCallbacks diagnostics;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!LIVETUNER_ENABLE_RUNTIME) return false;
        
        try {
            if (json_path.size() > internal::max_key_bytes ||
                static_cast<size_t>(std::count(json_path.begin(), json_path.end(), '.')) >= internal::max_config_depth)
                throw std::length_error("JSON path exceeds limits");
            json candidate = json_;
            set_value_by_path(candidate, json_path, value);
            size_t entries = 0;
            if (!within_limits(candidate, 0, entries) || !internal::json_within_limits(candidate.dump()))
                throw std::length_error("JSON value exceeds limits");
            json_.swap(candidate);
            last_error_ = {};
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
    bool load(bool force = true) {
        internal::DeferredCallbacks diagnostics;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!LIVETUNER_ENABLE_RUNTIME) {
            handle_error(ErrorType::Disabled, "Runtime tuning is disabled");
            return false;
        }
        try {
            const auto modified = internal::get_file_modify_time(file_path_);
            const auto now = std::chrono::steady_clock::now();
            if (!force && now - last_read_attempt_ < std::chrono::milliseconds(10) &&
                modified == last_modify_time_) return false;
            last_read_attempt_ = now;
            last_modify_time_ = modified;
            ErrorInfo error;
            auto content = internal::read_file_once(file_path_, error);
            if (!content && error.type == ErrorType::FileEmpty) content = std::string{};
            if (!content) {
                handle_error(error.type, error.message, true);
                return false;
            }
            // Periodic checks must not discard unsaved set() changes when the
            // actual on-disk content has not changed.
            if (has_disk_snapshot_ && *content == last_disk_content_) {
                last_error_ = {};
                return false;
            }
            if (!internal::json_within_limits(*content))
                throw std::length_error("JSON input exceeds limits");
            json candidate = content->empty() ? json::object() : json::parse(*content);
            size_t entries = 0;
            if (!within_limits(candidate, 0, entries))
                throw std::length_error("JSON value exceeds limits");
            const bool changed = candidate != json_;
            json_.swap(candidate);
            last_disk_content_ = std::move(*content);
            has_disk_snapshot_ = true;
            last_error_ = {};
            return changed;
            
        } catch (const std::exception& e) {
            handle_error(ErrorType::ParseError,
                        "Failed to read file: " + std::string(e.what()), true);
            return false;
        }
    }

    static bool within_limits(const json& value, size_t depth, size_t& entries) {
        if (++entries > internal::max_config_entries || depth > internal::max_config_depth) return false;
        if (value.is_number_float() && !std::isfinite(value.get<double>())) return false;
        if (value.is_string() && value.get_ref<const std::string&>().size() > internal::max_value_bytes) return false;
        if (value.is_structured()) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                if (value.is_object() && it.key().size() > internal::max_key_bytes) return false;
                if (!within_limits(it.value(), depth + 1, entries)) return false;
            }
        }
        return true;
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
            if (token.empty()) throw std::invalid_argument("Empty JSON path component");
            tokens.push_back(token);
        }
        if (tokens.empty() || path.back() == '.') throw std::invalid_argument("Invalid JSON path");
        
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
    void handle_error(ErrorType type, const std::string& message, bool only_if_changed = false) const {
        if (only_if_changed && last_error_.type == type && last_error_.message == message) return;
        last_error_ = ErrorInfo(type, message, file_path_);
        
        if (error_callback_) {
            internal::DeferredCallbacks::dispatch([callback = error_callback_, error = last_error_] {
                static thread_local bool delivering = false;
                if (delivering) return;
                delivering = true;
                try { callback(error); }
                catch (...) { delivering = false; throw; }
                delivering = false;
            });
        }
    }

private:
    std::string file_path_;
    std::unique_ptr<internal::FileWatcher> watcher_;
    mutable json json_;
    mutable std::mutex mutex_;
    mutable ErrorInfo last_error_;
    ErrorCallback error_callback_;
    std::chrono::steady_clock::time_point last_read_attempt_{};
    std::filesystem::file_time_type last_modify_time_{};
    std::string last_disk_content_;
    bool has_disk_snapshot_ = false;
};

/**
 * @brief Automatic binding helper using nlohmann/json
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
// Test Support
// ============================================================

/**
 * LIVETUNER_ENABLE_TEST_SUPPORT provides testing utilities:
 * TestFixture, ITuner/IParams interfaces, adapters, factories.
 * 
 * Usage: #define LIVETUNER_ENABLE_TEST_SUPPORT before including.
 */
#ifdef LIVETUNER_ENABLE_TEST_SUPPORT
#include "LiveTunerTest.h"
#endif

/*
 * ============================================================================
 * LICENSE INFORMATION
 * ============================================================================
 * 
 * This library includes picojson (2-clause BSD License).
 * Copyright 2009-2010 Cybozu Labs, Inc., 2011-2014 Kazuho Oku.
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
// Define LIVETUNER_IMPLEMENTATION in ONE source file before including.
// Exactly one translation unit must provide the implementation.
// ============================================================================

#ifdef LIVETUNER_IMPLEMENTATION

// ============================================================
// MSVC warning suppression
// ============================================================
// Suppress warnings from platform headers (Windows.h, etc.)
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

#ifndef _WIN32
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Platform-header warnings must not suppress warnings in our implementation.
#if defined(_MSC_VER)
#pragma warning(pop)
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

#ifndef _WIN32
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
#endif // !_WIN32

FileReadResult read_regular_file(const std::filesystem::path& path) {
    const auto failed = [&](ErrorType type, const char* message) {
        return FileReadResult{std::nullopt, ErrorInfo(type, message, path.string())};
    };
#ifdef _WIN32
    auto file = make_unique_invalid_handle(CreateFileW(path.wstring().c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        (LIVETUNER_STRICT_FILE_READS ? FILE_FLAG_OPEN_REPARSE_POINT : 0) | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE)
        return failed(GetLastError() == ERROR_FILE_NOT_FOUND ? ErrorType::FileNotFound : ErrorType::FileAccessDenied,
                      "Cannot open configuration file");
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(file.get()) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.get(), &info) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (LIVETUNER_STRICT_FILE_READS && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)))
        return failed(ErrorType::InvalidFormat, "Configuration is not a regular file permitted by the read policy");
    if (info.nFileSizeHigh != 0 || info.nFileSizeLow > max_config_bytes)
        return failed(ErrorType::InvalidFormat, "Configuration exceeds byte limit");
#else
    UniqueFd file(open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK |
        (LIVETUNER_STRICT_FILE_READS ? O_NOFOLLOW : 0)));
    if (!file) return failed(errno == ENOENT ? ErrorType::FileNotFound : ErrorType::FileAccessDenied,
                             "Cannot open configuration file");
    struct stat info{};
    if (fstat(file.get(), &info) != 0 || !S_ISREG(info.st_mode))
        return failed(ErrorType::InvalidFormat, "Configuration must be a regular non-link file");
    if (info.st_size < 0 || static_cast<uintmax_t>(info.st_size) > max_config_bytes)
        return failed(ErrorType::InvalidFormat, "Configuration exceeds byte limit");
#endif
    std::string content;
    std::array<char, 4096> buffer{};
    for (;;) {
        // Read at most one byte past the limit, never relying on the initial size.
        const auto capacity = std::min(buffer.size(), max_config_bytes - content.size() + 1);
#ifdef _WIN32
        DWORD count = 0;
        if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(capacity), &count, nullptr))
            return failed(ErrorType::FileReadError, "Configuration read failed");
#else
        const auto count = read(file.get(), buffer.data(), capacity);
        if (count < 0) {
            if (errno == EINTR) continue;
            return failed(ErrorType::FileReadError, "Configuration read failed");
        }
#endif
        if (count == 0) break;
        if (static_cast<size_t>(count) > max_config_bytes - content.size())
            return failed(ErrorType::InvalidFormat, "Configuration exceeds byte limit");
        content.append(buffer.data(), static_cast<size_t>(count));
    }
    if (content.empty()) return failed(ErrorType::FileEmpty, "Configuration is empty");
    return FileReadResult{std::move(content), {}};
}

bool write_regular_file(const std::filesystem::path& path, const std::string& content, ErrorInfo& error) {
    auto failed = [&](const char* message) {
        error = ErrorInfo(ErrorType::FileAccessDenied, message, path.string());
        return false;
    };
    if (content.size() > max_config_bytes) return failed("Configuration exceeds byte limit");
#ifdef _WIN32
    auto file = make_unique_invalid_handle(CreateFileW(path.wstring().c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) return failed("Cannot open existing configuration for writing");
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(file.get()) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.get(), &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) || info.nNumberOfLinks != 1)
        return failed("Refusing to write a non-regular or linked file");
    // Validate the opened handle before truncation, never a separately checked pathname.
    if (!SetEndOfFile(file.get())) return failed("Cannot truncate configuration");
    DWORD count = 0;
    if (!WriteFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &count, nullptr) || count != content.size())
        return failed("Configuration write failed");
#else
    UniqueFd file(open(path.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!file) return failed("Cannot open existing configuration for writing");
    struct stat info{};
    if (fstat(file.get(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1)
        return failed("Refusing to write a non-regular or linked file");
    if (ftruncate(file.get(), 0) != 0) return failed("Cannot truncate configuration");
    size_t offset = 0;
    while (offset < content.size()) {
        auto count = write(file.get(), content.data() + offset, content.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return failed("Configuration write failed");
        offset += static_cast<size_t>(count);
    }
#endif
    error = ErrorInfo{};
    return true;
}

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
FileWatcher::FileWatcher() : impl_(std::make_unique<Impl>()) {}

FileWatcher::FileWatcher(const FileWatcherConfig& config)
    : impl_(std::make_unique<Impl>()), config_(config) {
    config_.validate();
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::set_config(const FileWatcherConfig& config) {
    if (!running_.load()) {
        config_ = config;
        config_.validate();
    }
}

bool FileWatcher::start(const std::filesystem::path& file_path, ChangeCallback callback) {
    if (running_.load()) {
        stop();
    }

    file_path_ = file_path;
    callback_ = std::move(callback);
    running_.store(true);

    const bool started = start_native();
    if (!started) stop();
    return started;
}

void FileWatcher::stop() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    
    {
        std::lock_guard<std::mutex> lock(cv_mtx_);
        change_detected_.store(true);
    }
    cv_.notify_all();

    signal_native();

    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    stop_native();
}

bool FileWatcher::wait_for_change(std::chrono::milliseconds timeout) {
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

void FileWatcher::wait_for_change() {
    std::unique_lock<std::mutex> lock(cv_mtx_);
    cv_.wait(lock, [this] {
        return change_detected_.load() || !running_.load();
    });
    change_detected_.store(false);
}

bool FileWatcher::is_running() const {
    return running_.load();
}

bool FileWatcher::has_native_support() {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

void FileWatcher::notify_change() {
    {
        std::lock_guard<std::mutex> lock(cv_mtx_);
        change_detected_.store(true);
    }
    cv_.notify_all();
    
    if (callback_) {
        callback_();
    }
}

bool FileWatcher::start_polling() {
    watcher_thread_ = std::thread([this] { watch_polling(); });
    return true;
}

void FileWatcher::watch_polling() {
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
bool FileWatcher::start_native() {
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

    auto ready = std::make_shared<std::promise<bool>>();
    auto ready_result = ready->get_future();
    watcher_thread_ = std::thread([this, ready] {
        size_t buffer_size = impl_->current_buffer_size.load();
        std::vector<BYTE> buffer(buffer_size);
        OVERLAPPED overlapped{};
        auto overlapped_event = make_unique_handle(CreateEvent(nullptr, TRUE, FALSE, nullptr));
        if (!overlapped_event) {
            ready->set_value(false);
            return;
        }
        overlapped.hEvent = overlapped_event.get();

        auto filename = file_path_.filename().wstring();
        bool first_request = true;

        while (running_.load()) {
            ResetEvent(overlapped.hEvent);

            BOOL result = ReadDirectoryChangesW(
                impl_->dir_handle.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_FILE_NAME,
                nullptr,
                &overlapped,
                nullptr
            );
            if (first_request) {
                first_request = false;
                ready->set_value(result != FALSE);
                if (!result) return;
            }

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
                            } catch (const std::exception& e) {
                                log(LogLevel::Error, std::string("Watcher overflow handling failed: ") + e.what());
                            }
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
                // The OVERLAPPED and buffer must survive cancellation completion.
                CancelIoEx(impl_->dir_handle.get(), &overlapped);
                DWORD ignored = 0;
                GetOverlappedResult(impl_->dir_handle.get(), &overlapped, &ignored, TRUE);
                break;
            }
        }
    });
    // start() must not return before the first watch request is armed.
    return ready_result.get();
}

void FileWatcher::signal_native() {
    if (impl_->stop_event) {
        SetEvent(impl_->stop_event.get());
    }
}

void FileWatcher::stop_native() {
    impl_->dir_handle.reset();
    impl_->stop_event.reset();
}

#elif defined(__linux__)
bool FileWatcher::start_native() {
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
                        
                        if (event->mask & IN_Q_OVERFLOW) should_notify = true;
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

void FileWatcher::signal_native() {
    if (impl_->pipe_write_fd) {
        char c = 'x';
        [[maybe_unused]] auto _ = write(impl_->pipe_write_fd.get(), &c, 1);
    }
}

void FileWatcher::stop_native() {
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

bool FileWatcher::start_native() {
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

void FileWatcher::signal_native() {
    if (impl_->stream) {
        FSEventStreamStop(impl_->stream);
        FSEventStreamInvalidate(impl_->stream);
        // Invalidation prevents new delivery; drain already queued callbacks.
        if (impl_->queue) dispatch_sync_f(impl_->queue, nullptr, [](void*) {});
    }
}

void FileWatcher::stop_native() {
    if (impl_->stream) {
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
bool FileWatcher::start_native() {
    return start_polling();
}

void FileWatcher::stop_native() {
    // Nothing to do for polling mode
}
void FileWatcher::signal_native() {}
#endif

} // namespace internal

// ============================================================
// Params, LiveTuner, NlohmannParams - Destructor and Move Operations
// ============================================================
// These must be defined here because they use unique_ptr<FileWatcher>
// which requires complete type of FileWatcher::Impl

Params::~Params() {
    stop_watching();
}

Params::Params(Params&& other) noexcept
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
    , file_changed_(std::move(other.file_changed_))
    , last_error_(std::move(other.last_error_))
    , on_change_callback_(std::move(other.on_change_callback_))
    , in_callback_(other.in_callback_.load())
{
}

Params& Params::operator=(Params&& other) noexcept {
    if (this != &other) {
        stop_watching();
        std::scoped_lock lock(mtx_, other.mtx_);
        file_path_ = std::move(other.file_path_);
        format_ = other.format_;
        file_cache_ = std::move(other.file_cache_);
        bindings_ = std::move(other.bindings_);
        current_values_ = std::move(other.current_values_);
        file_watcher_ = std::move(other.file_watcher_);
        file_watcher_config_ = std::move(other.file_watcher_config_);
        file_read_retry_config_ = std::move(other.file_read_retry_config_);
        use_event_driven_ = other.use_event_driven_;
        file_changed_ = std::move(other.file_changed_);
        last_error_ = std::move(other.last_error_);
        on_change_callback_ = std::move(other.on_change_callback_);
        in_callback_.store(other.in_callback_.load());
    }
    return *this;
}

LiveTuner::~LiveTuner() {
    stop_async();
}

LiveTuner::LiveTuner(LiveTuner&& other) noexcept
    : mtx_()
    , input_file_path_(std::move(other.input_file_path_))
    , file_cache_(std::move(other.file_cache_))
    , file_watcher_config_(std::move(other.file_watcher_config_))
    , file_read_retry_config_(std::move(other.file_read_retry_config_))
    , use_event_driven_(other.use_event_driven_)
    , last_error_(std::move(other.last_error_))
    , async_tasks_(std::move(other.async_tasks_))
{
}

LiveTuner& LiveTuner::operator=(LiveTuner&& other) noexcept {
    if (this != &other) {
        stop_async();
        std::scoped_lock lock(mtx_, other.mtx_);
        input_file_path_ = std::move(other.input_file_path_);
        file_cache_ = std::move(other.file_cache_);
        file_watcher_config_ = std::move(other.file_watcher_config_);
        file_read_retry_config_ = std::move(other.file_read_retry_config_);
        use_event_driven_ = other.use_event_driven_;
        last_error_ = std::move(other.last_error_);
        async_tasks_ = std::move(other.async_tasks_);
    }
    return *this;
}

#ifdef LIVETUNER_USE_NLOHMANN_JSON
NlohmannParams::~NlohmannParams() {
    if (watcher_) {
        watcher_->stop();
    }
}
#endif // LIVETUNER_USE_NLOHMANN_JSON

} // namespace livetuner

#endif // LIVETUNER_IMPLEMENTATION
