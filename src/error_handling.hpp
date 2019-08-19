#ifndef SASS_ERROR_HANDLING_H
#define SASS_ERROR_HANDLING_H

// sass.hpp must go before all system headers to get the
// __EXTENSIONS__ fix on Solaris.
#include "sass.hpp"

#include <string>

#include "units.hpp"
#include "position.hpp"
#include "backtrace.hpp"
#include "ast_fwd_decl.hpp"
#include "sass/functions.h"

namespace Sass {

  struct Backtrace;

  namespace Exception {

    const char* const def_msg = "Invalid sass detected";
    const char* const def_op_msg = "Undefined operation";
    const char* const def_op_null_msg = "Invalid null operation";
    const char* const def_nesting_limit = "Code too deeply neested";

    class Base {
      protected:
        std::string msg;
        std::string prefix;
      public:
        ParserState pstate;
        Backtraces traces;
      public:
        Base(ParserState pstate, std::string msg, Backtraces traces);
        virtual ~Base() throw();
        virtual const char* errtype() const;
        virtual const char* what() const throw();
    };

    class InvalidSass : public Base {
      public:
        // Assumes that `this` will outlive `other`.
        InvalidSass(InvalidSass& other);

        // Required because the copy constructor's argument is not const.
        // Can't use `std::move` here because we build on Visual Studio 2013.
        InvalidSass(InvalidSass &&other);

        InvalidSass(ParserState pstate, Backtraces traces, std::string msg, char* owned_src = nullptr);
        virtual ~InvalidSass() throw();
        char *owned_src;
    };

    class InvalidParent : public Base {
      protected:
        Selector* parent;
        Selector* selector;
      public:
        InvalidParent(Selector* parent, Backtraces traces, Selector* selector);
        virtual ~InvalidParent() throw();
    };

    class MissingArgument : public Base {
      protected:
        std::string fn;
        std::string arg;
        std::string fntype;
      public:
        MissingArgument(ParserState pstate, Backtraces traces, std::string fn, std::string arg, std::string fntype);
        virtual ~MissingArgument() throw();
    };

    class InvalidArgumentType : public Base {
      protected:
        std::string fn;
        std::string arg;
        std::string type;
        const Value* value;
      public:
        InvalidArgumentType(ParserState pstate, Backtraces traces, std::string fn, std::string arg, std::string type, const Value* value = 0);
        virtual ~InvalidArgumentType() throw();
    };

    class InvalidVarKwdType : public Base {
      protected:
        std::string name;
        const Argument* arg;
      public:
        InvalidVarKwdType(ParserState pstate, Backtraces traces, std::string name, const Argument* arg = 0);
        virtual ~InvalidVarKwdType() throw();
    };

    class InvalidSyntax : public Base {
      public:
        InvalidSyntax(ParserState pstate, Backtraces traces, std::string msg);
        virtual ~InvalidSyntax() throw();
    };

    class NestingLimitError : public Base {
      public:
        NestingLimitError(ParserState pstate, Backtraces traces, std::string msg = def_nesting_limit);
        virtual ~NestingLimitError() throw();
    };

    class DuplicateKeyError : public Base {
      protected:
        const Map& dup;
        const Expression& org;
      public:
        DuplicateKeyError(Backtraces traces, const Map& dup, const Expression& org);
        virtual ~DuplicateKeyError() throw();
        virtual const char* errtype() const { return "Error"; }
    };

    class TypeMismatch : public Base {
      protected:
        const Expression& var;
        const std::string type;
      public:
        TypeMismatch(Backtraces traces, const Expression& var, const std::string type);
        virtual ~TypeMismatch() throw();
        virtual const char* errtype() const { return "Error"; }
    };

    class InvalidValue : public Base {
      protected:
        const Expression& val;
      public:
        InvalidValue(Backtraces traces, const Expression& val);
        virtual ~InvalidValue() throw();
        virtual const char* errtype() const { return "Error"; }
    };

    class StackError : public Base {
      protected:
        const AST_Node& node;
      public:
        StackError(Backtraces traces, const AST_Node& node);
        virtual ~StackError() throw();
        virtual const char* errtype() const { return "SystemStackError"; }
    };

    /* common virtual base class (has no pstate or trace) */
    class OperationError : public std::runtime_error {
      protected:
        std::string msg;
      public:
        OperationError(std::string msg = def_op_msg);
        virtual ~OperationError() throw();
        virtual const char* errtype() const { return "Error"; }
        virtual const char* what() const throw() { return msg.c_str(); }
    };

    class ZeroDivisionError : public OperationError {
      protected:
        const Expression& lhs;
        const Expression& rhs;
      public:
        ZeroDivisionError(const Expression& lhs, const Expression& rhs);
        virtual ~ZeroDivisionError() throw();
        virtual const char* errtype() const { return "ZeroDivisionError"; }
    };

    class IncompatibleUnits : public OperationError {
      protected:
        // const Sass::UnitType lhs;
        // const Sass::UnitType rhs;
      public:
        IncompatibleUnits(const Units& lhs, const Units& rhs);
        IncompatibleUnits(const UnitType lhs, const UnitType rhs);
        virtual ~IncompatibleUnits() throw();
    };

    class UndefinedOperation : public OperationError {
      protected:
        const Expression* lhs;
        const Expression* rhs;
        const Sass_OP op;
      public:
        UndefinedOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        // virtual const char* errtype() const { return "Error"; }
        virtual ~UndefinedOperation() throw();
    };

    class InvalidNullOperation : public UndefinedOperation {
      public:
        InvalidNullOperation(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        virtual ~InvalidNullOperation() throw();
    };

    class AlphaChannelsNotEqual : public OperationError {
      protected:
        const Expression* lhs;
        const Expression* rhs;
        const Sass_OP op;
      public:
        AlphaChannelsNotEqual(const Expression* lhs, const Expression* rhs, enum Sass_OP op);
        // virtual const char* errtype() const { return "Error"; }
        virtual ~AlphaChannelsNotEqual() throw();
    };

    class SassValueError : public Base {
    public:
      SassValueError(Backtraces traces, ParserState pstate, OperationError& err);
      virtual ~SassValueError() throw();
    };

    class TopLevelParent : public Base {
    public:
      TopLevelParent(Backtraces traces, ParserState pstate);
      virtual ~TopLevelParent() throw();
    };

    class UnsatisfiedExtend : public Base {
    public:
      UnsatisfiedExtend(Backtraces traces, Extension extension);
      virtual ~UnsatisfiedExtend() throw();
    };

    class ExtendAcrossMedia : public Base {
    public:
      ExtendAcrossMedia(Backtraces traces, Extension extension);
      virtual ~ExtendAcrossMedia() throw();
    };

  }

  void warn(std::string msg, ParserState pstate);
  void warn(std::string msg, ParserState pstate, Backtrace* bt);
  void warning(std::string msg, ParserState pstate);

  void deprecated_function(std::string msg, ParserState pstate);
  void deprecated(std::string msg, std::string msg2, bool with_column, ParserState pstate);
  void deprecated_bind(std::string msg, ParserState pstate);
  // void deprecated(std::string msg, ParserState pstate, Backtrace* bt);

  void coreError(std::string msg, ParserState pstate);
  void error(std::string msg, ParserState pstate, Backtraces& traces);

}

#endif
