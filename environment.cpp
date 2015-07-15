#include "ast.hpp"
#include "environment.hpp"

namespace Sass {

  template <typename T>
  Environment<T>::Environment() : local_frame_(unordered_map<string, T>()), parent_(0) { }
  template <typename T>
  Environment<T>::Environment(Environment<T>* env) : local_frame_(unordered_map<string, T>()), parent_(env) { }
  template <typename T>
  Environment<T>::Environment(Environment<T>& env) : local_frame_(unordered_map<string, T>()), parent_(&env) { }

  template <typename T>
  Environment<T>* Environment<T>::global_env()
  {
    Environment* cur = this;
    while (cur->is_lexical()) {
      cur = cur->parent_;
    }
    return cur;
  }

  template <typename T>
  Environment<T>* Environment<T>::lexical_env(const string& key)
  {
    Environment* cur = this;
    while (cur) {
      if (cur->has_local(key)) {
        return cur;
      }
      cur = cur->parent_;
    }
    return this;
  }

  template <typename T>
  void Environment<T>::del_global(const string& key)
  { global_env()->local_frame_.erase(key); }

  // see if we have a lexical variable
  // move down the stack but stop before we
  // reach the global frame (is not included)
  template <typename T>
  bool Environment<T>::has_lexical(const string& key) const
  {
    auto cur = this;
    while (cur->is_lexical()) {
      if (cur->has_local(key)) return true;
      cur = cur->parent_;
    }
    return false;
  }

  template <typename T>
  bool Environment<T>::has_global(const string& key)
  { return global_env()->has(key); }

  template <typename T>
  T& Environment<T>::get_global(const string& key)
  { return (*global_env())[key]; }

  // see if we have a lexical we could update
  // either update already existing lexical value
  // or if flag is set, we create one if no lexical found
  template <typename T>
  void Environment<T>::set_lexical(const string& key, T val)
  {
    auto cur = this;
    while (cur->is_lexical()) {
      if (cur->has_local(key)) {
        // cerr << "overwriting lexical\n";
        cur->set_local(key, val);
        return;
      }
      cur = cur->parent_;
    }
    set_local(key, val);
  }

  template <typename T>
  void Environment<T>::set_global(const string& key, T val)
  {
    if (global_env()->has_local(key)) {
      T& old = global_env()->get_local(key);
      if (global_env()->mem.has(old)) {
        if (old != val) {
          // cerr << "-- delete " << key << ": " << old << " from " << &mem << endl;
          global_env()->mem.destroy(old);
        }
      }
    }
    global_env()->local_frame_[key] = val;
  }

  // look on the full stack for key
  // include all scopes available
  template <typename T>
  bool Environment<T>::has(const string& key) const
  {
    auto cur = this;
    while (cur) {
      if (cur->has_local(key)) {
        return true;
      }
      cur = cur->parent_;
    }
    return false;
  }



  // link parent to create a stack
  template <typename T>
  void Environment<T>::link(Environment& env) { parent_ = &env; }
  template <typename T>
  void Environment<T>::link(Environment* env) { parent_ = env; }

  // this is used to find the global frame
  // which is the second last on the stack
  template <typename T>
  bool Environment<T>::is_lexical() const
  {
    return !! parent_ && parent_->parent_;
  }

  // only match the real root scope
  // there is still a parent around
  // not sure what it is actually use for
  // I guess we store functions etc. there
  template <typename T>
  bool Environment<T>::is_global() const
  {
    return parent_ && ! parent_->parent_;
  }

  // scope operates on the current frame

  template <typename T>
  unordered_map<string, T>& Environment<T>::local_frame() {
    return local_frame_;
  }

  template <typename T>
  bool Environment<T>::has_local(const string& key) const
  { return local_frame_.find(key) != local_frame_.end(); }

  template <typename T>
  T& Environment<T>::get_local(const string& key)
  { return local_frame_[key]; }

  template <typename T>
  void Environment<T>::set_local(const string& key, T val)
  {
    if (local_frame_.count(key)) {
      T& old = local_frame_[key];
      if (this->mem.has(old)) {
        if (old != val) {
          // cerr << "-- delete " << key << ": "  << old << " from " << &mem << endl;
          this->mem.destroy(old);
        }
      }
    }
    local_frame_[key] = val;
  }

  template <typename T>
  void Environment<T>::del_local(const string& key)
  { local_frame_.erase(key); }

    // use array access for getter and setter functions
  template <typename T>
  T& Environment<T>::operator[](const string& key)
  {
    auto cur = this;
    while (cur) {
      if (cur->has_local(key)) {
        return cur->get_local(key);
      }
      cur = cur->parent_;
    }
    return get_local(key);
  }

  template <typename T>
  size_t Environment<T>::print(string prefix)
  {
    size_t indent = 0;
    if (parent_) {
      indent = parent_->print(prefix) + 1;
    }
    cerr << prefix << string(indent, ' ') << "== " << this << endl;
    for (typename unordered_map<string, T>::iterator i = local_frame_.begin(); i != local_frame_.end(); ++i) {
      if (!ends_with(i->first, "[f]") && !ends_with(i->first, "[f]4") && !ends_with(i->first, "[f]2")) {
        cerr << prefix << string(indent, ' ') << i->first << " "  << i->second;
        if (Value* val = dynamic_cast<Value*>(i->second)) {
          cerr << " : " << val->to_string(true, 5);
        }
        cerr << endl;
      }
    }
    return indent ;
  }

  // compile implementation for AST_Node
  template class Environment<AST_Node*>;

}

