/* File: ast.cc
 * ------------
 */

#include "ast.h"
#include "ast_decl.h"
#include "ast_type.h"
#include <stdio.h>  // printf
#include <string.h> // strdup

Node::Node(yyltype loc) {
  location = new yyltype(loc);
  parent = NULL;
}

Node::Node() {
  location = NULL;
  parent = NULL;
}

Decl *Node::FindSymbol(const char *name) const {
  if (symbols)
    if (auto ret = symbols->Lookup(name))
      return ret;
  return nullptr;
}

Decl *Node::FindSymbolInParents(const char *name) const {
  if (symbols)
    if (auto ret = symbols->Lookup(name))
      return ret;
  if (parent)
    return parent->FindSymbolInParents(name);
  return nullptr;
}

Decl *Node::FindSymbolInClass(const char *name) const {
  if (symbols)
    if (auto ret = symbols->Lookup(name))
      return ret;
  if (parent)
    return parent->FindSymbolInClass(name);
  return nullptr;
}

Identifier::Identifier(yyltype loc, const char *n) : Node(loc) {
  name = strdup(n);
}
