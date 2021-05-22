/*****************************************************************************/
/* Part of LibSass, released under the MIT license (See LICENSE.txt).        */
/*****************************************************************************/
#include "fn_maps.hpp"

#include "compiler.hpp"
#include "ast_values.hpp"

namespace Sass {

  namespace Functions {

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    namespace Maps {

      /*******************************************************************/

      // Merges [map1] and [map2], with values in [map2] taking precedence.
      // If both [map1] and [map2] have a map value associated with
      // the same key, this recursively merges those maps as well.
      Map* deepMergeImpl(Map* map1, Map* map2)
      {

        if (map2->empty()) return map1;

        // Avoid making a mutable copy of `map2` if it would totally overwrite `map1`
        // anyway.
        // var mutable = false;
        // var result = map2.contents;
        // void _ensureMutable() {
        //   if (mutable) return;
        //   mutable = true;
        //   result = Map.of(result);
        // }

        MapObj result = SASS_MEMORY_COPY(map2);

        // Because values in `map2` take precedence over `map1`, we just check if any
        // entires in `map1` don't have corresponding keys in `map2`, or if they're
        // maps that need to be merged in their own right.
        for (auto kv : map1->elements()) {
          Value* key = kv.first;
          Value* value = kv.second;
          auto it = result->find(key);
          if (it != result->end()) {
            Map* valueMap = value->isaMap();
            Map* resultMap = it->second->isaMap();
            if (resultMap != nullptr && valueMap != nullptr) {
              Map* merged = deepMergeImpl(valueMap, resultMap);
              // if (identical(merged, resultMap)) return;
              it.value() = merged;
            }
          }
          else {
            result->insert(kv);
          }
        }

        return result.detach();
      }

      /*******************************************************************/

      BUILT_IN_FN(get)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);
        Value* key = arguments[1]->assertValue(compiler, Strings::key);
        auto it = map->find(key);
        if (it != map->end()) return it->second;
        return SASS_MEMORY_NEW(Null, pstate);
      }

      /*******************************************************************/

      BUILT_IN_FN(merge)
      {
        MapObj map1 = arguments[0]->assertMap(compiler, Strings::map1);
        MapObj map2 = arguments[1]->assertMap(compiler, Strings::map2);
        // We assign to ourself, so we can optimize this
        // This can shave off a few percent of run-time
        #ifdef SASS_OPTIMIZE_SELF_ASSIGN
        if (selfAssign && map1->refcount < AssignableRefCount + 1) {
          for (auto kv : map2->elements()) { map1->insertOrSet(kv); }
          return map1.detach();
        }
        #endif
        Map* copy = SASS_MEMORY_COPY(map1);
        for (auto kv : map2->elements()) {
          copy->insertOrSet(kv); }
        return copy;
      }

      /*******************************************************************/

      // Because the signature below has an explicit `$key` argument, it doesn't
      // allow zero keys to be passed. We want to allow that case, so we add an
      // explicit overload for it.
      BUILT_IN_FN(remove_one)
      {
        return arguments[0]->assertMap(compiler, Strings::map);
      }

      /*******************************************************************/

      BUILT_IN_FN(remove_many)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);

        #ifdef SASS_OPTIMIZE_SELF_ASSIGN
        if (selfAssign && map->refcount < AssignableRefCount + 1) {
          map->erase(arguments[1]);
          for (Value* key : arguments[2]->iterator()) {
            map->erase(key);
          }
          return map.detach();
        }
        #endif

        MapObj copy = SASS_MEMORY_COPY(map);
        copy->erase(arguments[1]);
        for (Value* key : arguments[2]->iterator()) {
          copy->erase(key);
        }
        return copy.detach();
      }

      /*******************************************************************/

      BUILT_IN_FN(keys)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);
        return SASS_MEMORY_NEW(List,
          pstate, map->keys(), SASS_COMMA);
      }

      /*******************************************************************/

      BUILT_IN_FN(values)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);
        return SASS_MEMORY_NEW(List,
          pstate, map->values(), SASS_COMMA);
      }

      /*******************************************************************/

      BUILT_IN_FN(hasKey)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);
        Value* key = arguments[1]->assertValue(compiler, Strings::key);
        return SASS_MEMORY_NEW(Boolean, pstate, map->has(key));
      }

      BUILT_IN_FN(fnDeepMerge)
      {
        Map* map1 = arguments[0]->assertMap(compiler, Strings::map1);
        Map* map2 = arguments[1]->assertMap(compiler, Strings::map2);
        return deepMergeImpl(map1, map2);
      }
      
      BUILT_IN_FN(fnDeepRemove)
      {
        MapObj map = arguments[0]->assertMap(compiler, Strings::map);
        MapObj result = SASS_MEMORY_COPY(map);
        auto it = arguments[1]->iterator();
        auto cur = it.begin(), end = it.end();
        Map* level = result;
        while (cur != end) {
          if (cur.isLast()) {
            level->erase(*cur);
            return result.detach();
          }
          else {
            // Go further down one key
            auto child = level->find(*cur);
            if (child == level->end()) return result.detach();
            auto childMap = child->second->isaMap();
            if (childMap == nullptr) return result.detach();
            child.value() = level = SASS_MEMORY_COPY(childMap);
          }
          ++cur;
        }
        return result.detach();
      }

      /*******************************************************************/

      void registerFunctions(Compiler& ctx)
      {
        Module& module(ctx.createModule("map"));
        module.addFunction("get", ctx.registerBuiltInFunction("map-get", "$map, $key", get));
        module.addFunction("merge", ctx.registerBuiltInFunction("map-merge", "$map1, $map2", merge));
        module.addFunction("remove", ctx.registerBuiltInOverloadFns("map-remove", {
          std::make_pair("$map", remove_one),
          std::make_pair("$map, $key, $keys...", remove_many)
          }));
        module.addFunction("keys", ctx.registerBuiltInFunction("map-keys", "$map", keys));
        module.addFunction("values", ctx.registerBuiltInFunction("map-values", "$map", values));
        module.addFunction("has-key", ctx.registerBuiltInFunction("map-has-key", "$map, $key", hasKey));

        module.addFunction("deep-merge", ctx.createBuiltInFunction("deep-merge", "$map1, $map2", fnDeepMerge));
        module.addFunction("deep-remove", ctx.createBuiltInFunction("deep-remove", "$map, $keys...", fnDeepRemove));

      }


      /*******************************************************************/

    }

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

  }

}
