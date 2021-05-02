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
#include "string_utils.hpp"

#include "debugger.hpp"

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

      BUILT_IN_FN(fnIf)
      {
        // Always evaluates both sides!
        return arguments[0]->isTruthy() ?
          arguments[1] : arguments[0];
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
          "units-level-3",
          "at-error",
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
        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            EnvRefs* module = pp->second.first;
            auto it = module->varIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->varIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasVar = false;
        for (auto global : parent->forwards) {
          if (global->varIdxs.count(variable->value()) != 0) {
            if (hasVar) {
              throw Exception::RuntimeException(compiler,
                "This variable is available from multiple global modules.");
            }
            hasVar = true;
          }
        }
        if (hasVar) return SASS_MEMORY_NEW(Boolean, pstate, true);
        EnvRef vidx = compiler.varRoot.findVarIdx(variable->value(), "", true);
        if (!vidx.isValid()) return SASS_MEMORY_NEW(Boolean, pstate, false);
        auto& var = compiler.varRoot.getVariable(vidx);
        return SASS_MEMORY_NEW(Boolean, pstate, !var.isNull());

      }

      /*******************************************************************/

      BUILT_IN_FN(variableExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        EnvRef vidx = compiler.varRoot.findVarIdx(variable->value(), "");

        bool hasVar = false;
        auto parent = compiler.getCurrentModule();
        for (auto global : parent->forwards) {
          if (global->varIdxs.count(variable->value()) != 0) {
            if (hasVar) {
              throw Exception::RuntimeException(compiler,
                "This variable is available from multiple global modules.");
            }
            hasVar = true;
          }
        }
        if (hasVar) return SASS_MEMORY_NEW(Boolean, pstate, true);
        if (!vidx.isValid()) return SASS_MEMORY_NEW(Boolean, pstate, false);
        auto& var = compiler.varRoot.getVariable(vidx);
        return SASS_MEMORY_NEW(Boolean, pstate, !var.isNull());
      }

      /*******************************************************************/

      BUILT_IN_FN(functionExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);
        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            EnvRefs* module = pp->second.first;
            auto it = module->fnIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->fnIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasFn = false;
        for (auto global : parent->forwards) {
          if (global->fnIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);
        EnvRef fidx = compiler.varRoot.findFnIdx(variable->value(), "");
        return SASS_MEMORY_NEW(Boolean, pstate, fidx.isValid());
      }

      /*******************************************************************/

      BUILT_IN_FN(mixinExists)
      {
        String* variable = arguments[0]->assertString(compiler, Sass::Strings::name);
        String* plugin = arguments[1]->assertStringOrNull(compiler, Sass::Strings::module);

        auto parent = compiler.getCurrentModule();
        if (plugin != nullptr) {
          auto pp = parent->module->moduse.find(plugin->value());
          if (pp != parent->module->moduse.end()) {
            EnvRefs* module = pp->second.first;
            auto it = module->mixIdxs.find(variable->value());
            return SASS_MEMORY_NEW(Boolean, pstate,
              it != module->mixIdxs.end());
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + plugin->value() + "\".");
          }
          return SASS_MEMORY_NEW(Boolean, pstate, false);
        }
        bool hasFn = false;
        for (auto global : parent->forwards) {
          if (global->mixIdxs.count(variable->value()) != 0) {
            if (hasFn) {
              throw Exception::RuntimeException(compiler,
                "This function is available from multiple global modules.");
            }
            hasFn = true;
          }
        }
        if (hasFn) return SASS_MEMORY_NEW(Boolean, pstate, true);

        auto fidx = compiler.varRoot.findMixIdx(variable->value(), "");
        return SASS_MEMORY_NEW(Boolean, pstate, fidx.isValid());
      }

      /*******************************************************************/

      BUILT_IN_FN(contentExists)
      {
        if (!eval.isInMixin()) {
          throw Exception::RuntimeException(compiler,
            "content-exists() may only be called within a mixin.");
        }
        return SASS_MEMORY_NEW(Boolean, pstate,
          eval.hasContentBlock());
      }

      /*******************************************************************/

      BUILT_IN_FN(moduleVariables)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        MapObj list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.getCurrentModule();
        auto it = module->module->moduse.find(ns->value());
        if (it != module->module->moduse.end()) {
          EnvRefs* refs = it->second.first;
          Module* root = it->second.second;
          if (root && !root->isCompiled) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->varIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            EnvRef vidx(refs->framePtr, entry.second);
            list->insert({ name, compiler.
              varRoot.getVariable(vidx) });
          }
          if (root)
          for (auto entry : root->mergedFwdVar) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            EnvRef vidx(0xFFFFFFFF, entry.second);
            list->insert({ name, compiler.
              varRoot.getVariable(vidx) });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list.detach();
      }

      /*******************************************************************/

      BUILT_IN_FN(moduleFunctions)
      {
        String* ns = arguments[0]->assertStringOrNull(compiler, Sass::Strings::module);
        MapObj list = SASS_MEMORY_NEW(Map, pstate);
        auto module = compiler.getCurrentModule();
        auto it = module->module->moduse.find(ns->value());
        if (it != module->module->moduse.end()) {
          EnvRefs* refs = it->second.first;
          Module* root = it->second.second;
          if (root && !root->isCompiled) {
            throw Exception::RuntimeException(compiler, "There is "
              "no module with namespace \"" + ns->value() + "\".");
          }
          for (auto entry : refs->fnIdxs) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            EnvRef fidx(refs->framePtr, entry.second);
            auto callable = compiler.varRoot.getFunction(fidx);
            auto fn = SASS_MEMORY_NEW(Function, pstate, callable);
            list->insert({ name, fn });
          }
          if (root)
          for (auto entry : root->mergedFwdFn) {
            auto name = SASS_MEMORY_NEW(String, pstate,
              sass::string(entry.first.norm()), true);
            EnvRef fidx(0xFFFFFFFF, entry.second);
            auto callable = compiler.varRoot.getFunction(fidx);
            auto fn = SASS_MEMORY_NEW(Function, pstate, callable);
            list->insert({ name, fn });
          }
        }
        else {
          throw Exception::RuntimeException(compiler, "There is "
            "no module with namespace \"" + ns->value() + "\".");
        }
        return list.detach();
      }

      /*******************************************************************/

      /// Like `_environment.findFunction`, but also returns built-in
      /// globally-available functions.
      Callable* _getFunction(const EnvKey& name, Compiler& ctx, const sass::string& ns = "") {
        EnvRef fidx = ctx.varRoot.findFnIdx(name, "");
        if (!fidx.isValid()) return nullptr;
        return ctx.varRoot.getFunction(fidx);
      }

      BUILT_IN_FN(findFunction)
      {

        String* name = arguments[0]->assertString(compiler, Sass::Strings::name);
        bool css = arguments[1]->isTruthy(); // supports all values
        String* ns = arguments[2]->assertStringOrNull(compiler, Sass::Strings::module);

        if (css && ns != nullptr) {
          throw Exception::RuntimeException(compiler,
            "$css and $module may not both be passed at once.");
        }

        if (css) {
          return SASS_MEMORY_NEW(Function, pstate, name->value());
        }

        CallableObj callable;

        auto parent = compiler.getCurrentModule();

        if (ns != nullptr) {
          auto pp = parent->module->moduse.find(ns->value());
          if (pp != parent->module->moduse.end()) {
            EnvRefs* module = pp->second.first;
            auto it = module->fnIdxs.find(name->value());
            if (it != module->fnIdxs.end()) {
              EnvRef fidx({ module->framePtr, it->second });
              callable = compiler.varRoot.getFunction(fidx);
            }
          }
          else {
            throw Exception::RuntimeException(compiler,
              "There is no module with the namespace \"" + ns->value() + "\".");
          }
        }
        else {

          callable = _getFunction(name->value(), compiler);

          if (!callable) {

            for (auto global : parent->forwards) {
              auto it = global->fnIdxs.find(name->value());
              if (it != global->fnIdxs.end()) {
                if (callable) {
                  throw Exception::RuntimeException(compiler,
                    "This function is available from multiple global modules.");
                }
                EnvRef fidx({ global->framePtr, it->second });
                callable = compiler.varRoot.getFunction(fidx);
                if (callable) break;
              }
            }
          }
        }


        if (callable == nullptr) {
          if (name->hasQuotes()) {
            throw
              Exception::RuntimeException(compiler,
                "Function not found: \"" + name->value() + "\"");
          }
          else {
            throw
              Exception::RuntimeException(compiler,
                "Function not found: " + name->value() + "");
          }
        }

        return SASS_MEMORY_NEW(Function, pstate, callable);

      }

      /*******************************************************************/

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
            FunctionExpression, pstate, str->value(), invocation,
            true); // Maybe pass flag into here!?
          return eval.acceptFunctionExpression(expression);

        }

        Function* fn = function->assertFunction(compiler, "function");
        if (fn->cssName().empty()) {
          return fn->callable()->execute(eval, invocation, pstate);
        }
        else {
          sass::string strm;
          strm += fn->cssName();
          eval.renderArgumentInvocation(strm, invocation);
          return SASS_MEMORY_NEW(
            String, fn->pstate(),
            std::move(strm));
        }


      }

      /*******************************************************************/

      BUILT_IN_FN(loadCss)
      {
        String* url = arguments[0]->assertStringOrNull(compiler, Strings::url);
        MapObj withMap = arguments[1]->assertMapOrNull(compiler, Strings::with);

        bool hasWith = withMap && !withMap->empty();

        EnvKeyFlatMap<ValueObj> config;
        sass::vector<WithConfigVar> withConfigs;

        if (hasWith) {
          for (auto& kv : withMap->elements()) {
            String* name = kv.first->assertString(compiler, "with key");
            EnvKey kname(name->value());
            WithConfigVar kvar;
            kvar.name = name->value();
            kvar.value = kv.second;
            kvar.isGuarded = false;
            kvar.wasUsed = false;
            kvar.pstate2 = name->pstate();
            kvar.isNull = !kv.second || kv.second->isaNull();
            withConfigs.push_back(kvar);
            if (config.count(kname) == 1) {
              throw Exception::RuntimeException(compiler,
                "The variable $" + kname.norm() + " was configured twice.");
            }
            config[name->value()] = kv.second;
          }
        }


        if (StringUtils::startsWith(url->value(), "sass:", 5)) {

          if (hasWith) {
            throw Exception::RuntimeException(compiler, "Built-in "
              "module " + url->value() + " can't be configured.");
          }

          return SASS_MEMORY_NEW(Null, pstate);
        }

        WithConfig wconfig(compiler.wconfig, withConfigs, hasWith);

        WithConfig*& pwconfig(compiler.wconfig);
        LOCAL_PTR(WithConfig, pwconfig, &wconfig);

        sass::string prev(pstate.getAbsPath());
        if (Root* sheet = eval.loadModule(
          prev, url->value(), false)) {
          if (!sheet->isCompiled) {
            ImportStackFrame iframe(compiler, sheet->import);
            LocalOption<bool> scoped(compiler.hasWithConfig,
              compiler.hasWithConfig || hasWith);
            eval.compileModule(sheet);
            wconfig.finalize(compiler);
          }
          else if (compiler.hasWithConfig || hasWith) {
            throw Exception::ParserException(compiler,
              sass::string(sheet->pstate().getImpPath())
              + " was already loaded, so it "
              "can't be configured using \"with\".");
          }
          eval.insertModule(sheet);
        }

        return SASS_MEMORY_NEW(Null, pstate);
      }

      /*******************************************************************/

      void registerFunctions(Compiler& compiler)
	    {

        BuiltInMod& module(compiler.createModule("meta"));

        compiler.registerBuiltInFunction(key_if, "$condition, $if-true, $if-false", fnIf);

        module.addMixin(key_load_css, compiler.createBuiltInMixin(key_load_css, "$url, $with: null", loadCss));
        module.addFunction(key_feature_exists, compiler.registerBuiltInFunction(key_feature_exists, "$feature", featureExists));
        module.addFunction(key_type_of, compiler.registerBuiltInFunction(key_type_of, "$value", typeOf));
        module.addFunction(key_inspect, compiler.registerBuiltInFunction(key_inspect, "$value", inspect));
        module.addFunction(key_keywords, compiler.registerBuiltInFunction(key_keywords, "$args", keywords));
        module.addFunction(key_global_variable_exists, compiler.registerBuiltInFunction(key_global_variable_exists, "$name, $module: null", globalVariableExists));
        module.addFunction(key_variable_exists, compiler.registerBuiltInFunction(key_variable_exists, "$name", variableExists));
        module.addFunction(key_function_exists, compiler.registerBuiltInFunction(key_function_exists, "$name, $module: null", functionExists));
        module.addFunction(key_mixin_exists, compiler.registerBuiltInFunction(key_mixin_exists, "$name, $module: null", mixinExists));
        module.addFunction(key_content_exists, compiler.registerBuiltInFunction(key_content_exists, "", contentExists));
        module.addFunction(key_module_variables, compiler.createBuiltInFunction(key_module_variables, "$module", moduleVariables));
        module.addFunction(key_module_functions, compiler.registerBuiltInFunction(key_module_functions, "$module", moduleFunctions));
        module.addFunction(key_get_function, compiler.registerBuiltInFunction(key_get_function, "$name, $css: false, $module: null", findFunction));
        module.addFunction(key_call, compiler.registerBuiltInFunction(key_call, "$function, $args...", call));

	    }

      /*******************************************************************/

    }

  }

}
