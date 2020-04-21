/* File: ast.h
 * -----------
 * This file defines the abstract base class Node and the concrete
 * Identifier and Error node subclasses that are used through the tree as
 * leaf nodes. A parse tree is a hierarchical collection of ast nodes (or,
 * more correctly, of instances of concrete subclassses such as VarDecl,
 * ForStmt, and AssignExpr).
 *
 * Location: Each node maintains its lexical location (line and columns in
 * file), that location can be NULL for those nodes that don't care/use
 * locations. The location is typcially set by the node constructor.  The
 * location is used to provide the context when reporting semantic errors.
 *
 * Parent: Each node has a pointer to its parent. For a Program node, the
 * parent is NULL, for all other nodes it is the pointer to the node one level
 * up in the parse tree.  The parent is not set in the constructor (during a
 * bottom-up parse we don't know the parent at the time of construction) but
 * instead we wait until assigning the children into the parent node and then
 * set up links in both directions. The parent link is typically not used
 * during parsing, but is more important in later phases.
 *
 * Code generation: For pp4 you are adding "Emit" behavior to the ast
 * node classes. Your code generator should do an postorder walk on the
 * parse tree, and when visiting each node, emitting the necessary
 * instructions for that construct.

 */

#ifndef _H_ast
#define _H_ast

#include "errors.h"
#include "hashtable.h"
#include "list.h"
#include "location.h"
#include <iostream>
#include <stdlib.h> // for NULL
#include <type_traits>

class CodeGenerator;
class Decl;
class Type;

class Node {
protected:
  yyltype *location;
  Node *parent;
  Hashtable<Decl *> *symbols;

public:
  Node(yyltype loc);
  Node();

  virtual void Emit() {}

  yyltype *GetLocation() const { return location; }
  void SetParent(Node *p) { parent = p; }
  Node *GetParent() const { return parent; }

  virtual char *GetName() const { return nullptr; }

  Decl *FindSymbol(const char *name) const;
  Decl *FindSymbolInParents(const char *name) const;
  virtual Decl *FindSymbolInClass(const char *name) const;

  template <typename T> void CollectSymbols(List<T> *list);
  template <typename T> T FindParentByType() const;
};

template <typename T> void Node::CollectSymbols(List<T> *list) {
  symbols = new Hashtable<Decl *>();
  for (auto &elem : list->Get()) {
    auto name = elem->GetName();
    auto prev = symbols->Lookup(name);
    if (prev != nullptr)
      ReportError::DeclConflict(elem, prev);
    else
      symbols->Enter(name, elem);
  }
}

template <typename T> T Node::FindParentByType() const {
  if (!parent)
    return nullptr;
  if (auto decl = dynamic_cast<T>(parent))
    return decl;
  return parent->FindParentByType<T>();
}

class Identifier : public Node {
protected:
  char *name;

public:
  Identifier(yyltype loc, const char *name);
  friend std::ostream &operator<<(std::ostream &out, Identifier *id) {
    return out << id->name;
  }

  char *GetName() const { return name; }
};

// This node class is designed to represent a portion of the tree that
// encountered syntax errors during parsing. The partial completed tree
// is discarded along with the states being popped, and an instance of
// the Error class can stand in as the placeholder in the parse tree
// when your parser can continue after an error.
class Error : public Node {
public:
  Error() : Node() {}
};

#endif
