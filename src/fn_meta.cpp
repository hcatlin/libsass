/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "fn_meta.hpp"

#include "eval.hpp"
#include "compiler.hpp"
#include "exceptions.hpp"
#include "ast_values.hpp"
#include "ast_callables.hpp"
#include "ast_expressions.hpp"

namespace Sass {

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Meta {

      /*******************************************************************/

      BUILT_IN_FN(typeOf)
      {
        sass::string copy(arguments[0]->type());
        return SASS_MEMORY_NEW(String,
          pstate, std::move(copy));
      }

      /*******************************************************************/

      BUILT_IN_FN(inspect)
      {
        if (arguments[0] == nullptr) {
          return SASS_MEMORY_NEW(
            String, pstate, "null");
        }
        return SASS_MEMORY_NEW(String,
          pstate, arguments[0]->inspect());
      }

      /*******************************************************************/

      BUILT_IN_FN(keywords)
      {
        ArgumentList* argumentList = arguments[0]->assertArgumentList(compiler, Sass::Strings::args);
        const ValueFlatMap& keywords = argumentList->keywords();
        MapObj map = SASS_MEMORY_NEW(Map, arguments[0]->pstate());
        for (auto kv : keywords) {
          sass::string key = kv.first.norm(); // .substr(1);
          // Util::ascii_normalize_underscore(key);
          // Wrap string key into a sass value
          map->insert(SASS_MEMORY_NEW(String,
            kv.second->pstate(), std::move(key)
          ), kv.second);
        }
        return map.detach();
      }

      /*******************************************************************/

      BUILT_IN_FN(featureExists)
      {
        String* feature = arguments[0]->assertString(compiler, "feature");
        static const auto* const features =
          new std::unordered_set<sass::string>{
          "global-variable-shadowing",
          "extend-selector-pseudoclass",
          "at-error",
          "units-level-3",
          "custom-property"
        };
        sass::string name(feature->value());
        return SASS_MEMORY_NEW(Boolean,
          pstate, features->count(name) == 1);
      }

      /*******************************************************************/

      BUILT_IN_FN(globalVariableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::RuntimeException(compiler,
            "Modules are not supported yet");
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          compiler.getVariable(variable->value(), true));

      }

      /*******************************************************************/

      BUILT_IN_FN(variableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        ValueObj ex = compiler.getVariable(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !ex.isNull());
      }

      BUILT_IN_FN(functionExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::RuntimeException(compiler,
            "Modules are not supported yet");
        }
        CallableObj fn = compiler.getFunction(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(mixinExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        if (plugin != nullptr) {
          throw Exception::RuntimeException(compiler,
            "Modules are not supported yet");
        }
        CallableObj fn = compiler.getMixin(variable->value());
        return SASS_MEMORY_NEW(Boolean, pstate, !fn.isNull());
      }

      BUILT_IN_FN(contentExists)
      {
        if (!eval.isInMixin()) {
          throw Exception::RuntimeException(compiler,
            "content-exists() may only be called within a mixin.");
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          eval.hasContentBlock());
      }

      BUILT_IN_FN(moduleVariables)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        Map* list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.varStack.back()->getModule();
        auto it = module->fwdModule33.find(ns->value());
        if (it != module->fwdModule33.end()) {
          VarRefs* refs = it->second.first;
          Root* root = it->second.second;
          if (root && !root->isActive) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->varIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef vidx(refs->varFrame, entry.second);
            list->insert({ name, compiler.
              varRoot.getVariable(vidx) });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list;
      }

      BUILT_IN_FN(moduleFunctions)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        Map* list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.varStack.back()->getModule();
        auto it = module->fwdModule33.find(ns->value());
        if (it != module->fwdModule33.end()) {
          VarRefs* refs = it->second.first;
          Root* root = it->second.second;
          if (root && !root->isActive) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->fnIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            VarRef fidx(refs->fnFrame, entry.second);
            auto callable = compiler.varRoot.getFunction(fidx);
            auto fn = SASS_MEMORY_NEW(Function, pstate, callable);
            list->insert({ name, fn });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list;
      }

      /// Like `_environment.getFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Context& ctx, const sass::string& ns = "") {
        return ctx.getFunction(name); // no detach, is a reference anyway
      }

      BUILT_IN_FN(getFunction)
      {

        String* name = arguments[0]->assertString(compiler, Sass::Strings::name);
        bool css = arguments[1]->isTruthy(); // supports all values
        String* plugin = arguments[2]->assertStringOrNull(compiler, Sass::Strings::module);

        if (css && plugin != nullptr) {
          throw Exception::RuntimeException(compiler,
            "$css and $module may not both be passed at once.");
        }

        CallableObj callable = css
          ? SASS_MEMORY_NEW(PlainCssCallable, pstate, name->value())
          : _getFunction(name->value(), compiler);

        if (callable == nullptr) throw
          Exception::RuntimeException(compiler,
            "Function not found: " + name->value());

        return SASS_MEMORY_NEW(Function, pstate, callable);

      }

      BUILT_IN_FN(call)
      {

        Value* function = arguments[0]->assertValue(compiler, "function");
        ArgumentList* args = arguments[1]->assertArgumentList(compiler, Sass::Strings::args);

        // ExpressionVector positional,
        //   EnvKeyMap<ExpressionObj> named,
        //   Expression* restArgs = nullptr,
        //   Expression* kwdRest = nullptr);

        ValueExpression* restArg = SASS_MEMORY_NEW(
          ValueExpression, args->pstate(), args);

        ValueExpression* kwdRest = nullptr;
        if (!args->keywords().empty()) {
          Map* map = args->keywordsAsSassMap();
          kwdRest = SASS_MEMORY_NEW(
            ValueExpression, map->pstate(), map);
        }

        ArgumentInvocationObj invocation = SASS_MEMORY_NEW(
          ArgumentInvocation, pstate, ExpressionVector{}, {}, restArg, kwdRest);

        if (String * str = function->isaString()) {
          sass::string name = str->value();
          compiler.addDeprecation(
            "Passing a string to call() is deprecated and will be illegal in LibSass "
            "4.1.0. Use call(get-function(" + str->inspect() + ")) instead.",
            str->pstate());

          InterpolationObj itpl = SASS_MEMORY_NEW(Interpolation, pstate);
          itpl->append(SASS_MEMORY_NEW(String, pstate, sass::string(str->value())));
          FunctionExpressionObj expression = SASS_MEMORY_NEW(
            FunctionExpression, pstate, itpl, invocation);
          return eval.acceptFunctionExpression(expression);
          // return expression->accept(&eval);

        }

        Callable* callable = function->assertFunction(compiler, "function")->callable();
        return callable->execute(eval, invocation, pstate, false);


      }

	    void registerFunctions(Compiler& compiler)
	    {

        Module& module(compiler.createModule("meta"));

		    // Meta functions
        module.addFunction("feature-exists", compiler.registerBuiltInFunction("feature-exists", "$feature", featureExists));
        module.addFunction("type-of", compiler.registerBuiltInFunction("type-of", "$value", typeOf));
        module.addFunction("inspect", compiler.registerBuiltInFunction("inspect", "$value", inspect));
        module.addFunction("keywords", compiler.registerBuiltInFunction("keywords", "$args", keywords));

		    // ToDo: dart-sass keeps them on the local environment scope, see below:
		    // These functions are defined in the context of the evaluator because
		    // they need access to the [_environment] or other local state.
        module.addFunction("global-variable-exists", compiler.registerBuiltInFunction("global-variable-exists", "$name, $module: null", globalVariableExists));
        module.addFunction("variable-exists", compiler.registerBuiltInFunction("variable-exists", "$name", variableExists));
        module.addFunction("function-exists", compiler.registerBuiltInFunction("function-exists", "$name, $module: null", functionExists));
        module.addFunction("mixin-exists", compiler.registerBuiltInFunction("mixin-exists", "$name, $module: null", mixinExists));
        module.addFunction("content-exists", compiler.registerBuiltInFunction("content-exists", "", contentExists));
		    module.addFunction("module-variables", compiler.createBuiltInFunction("module-variables", "$module", moduleVariables));
		    module.addFunction("module-functions", compiler.registerBuiltInFunction("module-functions", "$module", moduleFunctions));
        module.addFunction("get-function", compiler.registerBuiltInFunction("get-function", "$name, $css: false, $module: null", getFunction));
        module.addFunction("call", compiler.registerBuiltInFunction("call", "$function, $args...", call));

	    }

    }

  }

}
