/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * (c) 2014 Henner Zeller <h.zeller@acm.org>
 * Creative commons BY-SA
 */
#ifndef SHELL_EXTRUDE_CONFIG_VALUES_H_
#define SHELL_EXTRUDE_CONFIG_VALUES_H_

#include <string>

#include "multi-shell-extrude.h"   // Definition of Vector2D

// Prints usage of paramters to STDERR.
// Always returns 1 (convenient to return from main()).
int ParameterUsage(const char *progname);

// Set all parameters from commandline. Returns 'true' on success.
bool SetParametersFromCommandline(int argc, char *argv[]);
// TODO, others like SetParameterFromConfigFile()

// Classes to deal with configuration parameters. In general we want the
// parameters look like read-only values (they all have operator T())),
// that can be configured centrally.

class Parameter {
public:
  // Set parameter with default value, option name (can be NULL if no long
  // option), option char (short option), and helptext for user to show.
  Parameter(const char *short_name, char option_char, const char *helptext);
  virtual ~Parameter() {}

  virtual bool FromString(const char *s) = 0;  // parsing from some config input
  virtual std::string ToString() const = 0;    // print defaults.
  virtual bool RequiresValue() const = 0;

public:
  // public accessible const values.
  const char *const option_name;
  const char option_char;
  const char *const helptext;
};

// Fake parameter - placeholder for a headline displayed in list
// of parameters.
class ParamHeadline : public Parameter {
public:
  ParamHeadline(const char *title) : Parameter(NULL, 0, title) {}
  virtual bool FromString(const char *s) { return false; }
  virtual std::string ToString() const { return ""; }
  virtual bool RequiresValue() const { return false; }
};
#define ParamHeadline(title) char need_to_use_ParamHeadline_with_instance[-1]

template <class T> class TypedParameter : public Parameter {
public:
  TypedParameter(T default_value, const char *option_name,
                 char option_char, const char *helptext)
    : Parameter(option_name, option_char, helptext),
      value_(default_value) {
  }

  virtual bool FromString(const char *s);
  virtual std::string ToString() const;
  virtual bool RequiresValue() const;

  // Make it possible to use value as if it was a value of that type.
  operator const T&() const { return value_; }
  const T& get() const { return value_; }

  // Writable access to component values.
  T* operator->() { return &value_; }

  // Simple writable access to scalars.
  void operator=(const T &v) { value_ = v; }

private:
  // Don't allow assignment or default constructors.
  TypedParameter();
  TypedParameter(const TypedParameter<T> &);

  T value_;
};

// Parameter types we support.
typedef TypedParameter<std::string> StringParam;
typedef TypedParameter<int> IntParam;
typedef TypedParameter<float> FloatParam;
typedef TypedParameter<bool> BoolParam;
typedef TypedParameter<Vector2D> Vector2DParam;

#endif  // SHELL_EXTRUDE_CONFIG_VALUES_H_

