//
// Created by Ramsay on 2019/9/9.
//

#include "dom.hpp"

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

crawler::Nodes crawler::Node::getElementsByTag(const std::string &tagName) {
  return getElementsByPredicate([&tagName](const Node &node) -> bool {
    return node.getElementData().getTagName() == tagName;
  });
}

crawler::Node crawler::Node::getElementById(const std::string &id) {
  Nodes elements = getElementsByPredicate([&id](const Node &node) -> bool {
    return node.getElementData().id() == id;
  });
  assert(elements.size() == 1);
  return elements[0];
}
crawler::Nodes crawler::Node::select(Evaluator *evaluator) {
  return getElementsByPredicate([&evaluator, this](const Node &node) -> bool {
    return evaluator->matches(*this, node);
  });
}

crawler::Nodes crawler::Node::select(const std::string &cssQuery) {
  crawler::QueryParser cssParser(cssQuery);
  return select(*cssParser.parse());
}
bool crawler::Node::isElement() const { return nodeType == NodeType ::Element; }

bool crawler::Node::isText() const { return nodeType == NodeType ::Text; }

const crawler::ElementData &crawler::Node::getElementData() const {
  return std::get<ElementData>(nodeData);
}
const std::string &crawler::Node::getText() const {
  return std::get<std::string>(nodeData);
}

const std::shared_ptr<crawler::Node> &crawler::Node::getParent() const {
  return parent;
}

std::string crawler::ElementData::clazz() const {
  auto clazz = attributes.find("class");
  std::string classValue;
  if (clazz != attributes.end()) {
    return clazz->second;
  }
  return classValue;
}
std::string crawler::ElementData::id() const {
  auto result = attributes.find("id");
  if (result != attributes.end()) {
    return result->second;
  } else {
    return std::string("");
  }
}

crawler::TokenQueue::TokenQueue(std::string data, size_t pos)
    : data(std::move(data)), pos(pos) {}

bool crawler::TokenQueue::consumeWhiteSpace() {
  bool seen = false;
  while (matchesWhitespace()) {
    pos++;
    seen = true;
  }
  return seen;
}

bool crawler::TokenQueue::matchesWhitespace() {
  return !eof() && isspace(data.c_str()[pos]);
}

bool crawler::TokenQueue::eof() { return pos >= data.size(); }

bool crawler::TokenQueue::matches(const std::string &seq) {
  const std::string source = data.substr(pos, seq.size());
  return source == seq;
}

bool crawler::TokenQueue::matchesChomp(const std::string &seq) {
  if (matches(seq)) {
    pos += seq.size();
    return true;
  } else {
    return false;
  }
}
crawler::TokenQueue::TokenQueue(std::string data)
    : data(std::move(data)), pos(0) {}

const std::string &crawler::TokenQueue::getData() const { return data; }

size_t crawler::TokenQueue::getPos() const { return pos; }
std::string crawler::TokenQueue::consumeCssIdentifier() {
  return consumeByPredicate([this]() -> bool {
    return matchesAny(std::array<char, 2>({'-', '_'}));
  });
}

template <typename Predicate>
std::string crawler::TokenQueue::consumeByPredicate(Predicate predicate) {
  size_t start = pos;
  while (!eof() && (matchesWords() || predicate())) {
    pos++;
  }
  const size_t length = pos -start;
  return data.substr(start, length);
}

std::string crawler::TokenQueue::consumeElementSelector() {
  return consumeByPredicate([this]() -> bool {
    return matchesAny(std::array<std::string, 4>({"*|", "|", "_", "-"}));
  });
}

bool crawler::TokenQueue::matchesWords() {
  return !eof() && isalnum(data.c_str()[pos]);
}

template <size_t N>
bool crawler::TokenQueue::matchesAny(std::array<char, N> seq) {
  if (eof()) {
    return false;
  }
  return std::any_of(seq.cbegin(), seq.cend(),
                     [this](const char c) { return data.c_str()[pos] == c; });
}
template <size_t N>
bool crawler::TokenQueue::matchesAny(std::array<std::string, N> seq) {
  for (auto const &s : seq) {
    if (matches(s)) {
      return true;
    }
  }
  return false;
}
std::string crawler::TokenQueue::chompBalanced(char open, char close) {
  int start = -1;
  int end = -1;
  int depth = 0;
  char last = 0;
  bool isSingleQuote = false;
  bool isDoubleQuote = false;
  do {
    if (eof()) {
      break;
    }
    char c = consume();
    if (last != ESC) {
      if (c == '\'' && c != open && !isDoubleQuote) {
        isSingleQuote = !isSingleQuote;
      } else if (c == '\"' && c != open && !isSingleQuote) {
        isDoubleQuote = !isDoubleQuote;
      }
      if (isSingleQuote || isDoubleQuote) {
        continue;
      }
      if (c == open) {
        depth++;
        if (start == -1) {
          start = pos;
        }
      } else if (c == close) {
        depth--;
      }
    }
    if (depth > 0 && last != 0) {
      // don't include the outer match pair in the return
      end = pos;
    }
    last = c;
  } while (depth > 0);
  const int length = end - start;
  const std::string result = (end >= 0) ? data.substr(start, length) : "";
  if (depth > 0) {
    // throw exception?
  }
  return result;
}

char crawler::TokenQueue::consume() { return data.c_str()[pos++]; }

std::shared_ptr<crawler::Evaluator *> crawler::QueryParser::parse() {
  // not support combinator for now.
  tokenQueue.consumeWhiteSpace();
  if (tokenQueue.matchesAny(COMBINATORS)) {
    combinator(tokenQueue.consume());
  } else {
    findElements();
  }
  while (!tokenQueue.eof()) {
    bool seenWhiteSpace = tokenQueue.consumeWhiteSpace();
    if (tokenQueue.matchesAny(COMBINATORS)) {
      combinator(tokenQueue.consume());
    } else if (seenWhiteSpace) {
      combinator(' ');
    } else {
      // E.class, E#id, E[attr] etc. AND
      findElements();
    }
  }
  if (evals.size() == 1) {
    return std::make_shared<crawler::Evaluator *>(evals.front());
  } else {
    crawler::And andEvaluator(evals);
    return std::make_shared<crawler::Evaluator *>(&andEvaluator);
  }
}

std::shared_ptr<crawler::Evaluator *>
crawler::QueryParser::parse(const std::string &cssQuery) {
  QueryParser queryParser(cssQuery);
  return queryParser.parse();
}

void crawler::QueryParser::findElements() {
  if (tokenQueue.matchesChomp("#")) {
    findById();
  } else if (tokenQueue.matchesChomp(".")) {
    findByClass();
  } else if (tokenQueue.matchesWords()) {
    findByTag();
  }
}
void crawler::QueryParser::findById() {
  const std::string id = tokenQueue.consumeCssIdentifier();
  evals.emplace_back(new Id(id));
}
void crawler::QueryParser::findByClass() {
  const std::string clazz = tokenQueue.consumeCssIdentifier();
  evals.emplace_back(new Class(clazz));
}

void crawler::QueryParser::findByTag() {
  std::string tagName = tokenQueue.consumeElementSelector();
  evals.emplace_back(new Tag(tagName));
}

void crawler::QueryParser::combinator(char combinator) {
  tokenQueue.consumeWhiteSpace();
  std::string subQuery = tokenQueue.consumeSubQuery();
  Evaluator *currentEval;
  std::shared_ptr<Evaluator *> newEval = crawler::QueryParser::parse(subQuery);
  if (evals.size() == 1) {
    currentEval = evals.at(0);
  } else {
    currentEval = new And(evals);
  }
  evals.clear();
  if (combinator == '>') {
    Evaluator *immediateParent = new ImmediateParent(
        std::make_shared<crawler::Evaluator *>(currentEval));
    currentEval = new And({*newEval, immediateParent});
  } else if (combinator == ' ') {
    Evaluator *parent =
        new Parent(std::make_shared<crawler::Evaluator *>(currentEval));
    currentEval = new And({*newEval, parent});
  } else {
    throw std::runtime_error("Unknown combinator");
  }
  evals.emplace_back(currentEval);
}

std::string crawler::TokenQueue::consumeSubQuery() {
  std::stringstream buffer;
  while (!eof()) {
    if (matches("(")) {
      buffer << "(" << chompBalanced('(', ')') << ")";
    } else if (matches("[")) {
      buffer << "[" << chompBalanced('[', ']') << "]";
    } else if (matchesAny(crawler::QueryParser::COMBINATORS)) {
      break;
    } else {
      buffer << consume();
    }
  }
  return buffer.str();
}

crawler::QueryParser::QueryParser(const std::string &queryString)
    : queryString(queryString), tokenQueue(queryString) {}

crawler::Id::Id(std::string id) : id(std::move(id)) {}
bool crawler::Id::matches(const crawler::Node &root,
                          const crawler::Node &node) {
  return id == node.getElementData().id();
}
crawler::Class::Class(std::string clazz) : clazz(std::move(clazz)) {}
bool crawler::Class::matches(const crawler::Node &root,
                             const crawler::Node &node) {
  return clazz == node.getElementData().clazz();
}
crawler::Tag::Tag(std::string tagName) : tagName(std::move(tagName)) {}
bool crawler::Tag::matches(const crawler::Node &root,
                           const crawler::Node &node) {
  return tagName == node.getElementData().getTagName();
}
crawler::CombiningEvaluator::CombiningEvaluator(
    std::vector<Evaluator *> evalutors)
    : evalutors(std::move(evalutors)) {}

bool crawler::And::matches(const crawler::Node &root,
                           const crawler::Node &node) {
  return std::all_of(
      this->evalutors.cbegin(), this->evalutors.cend(),
      [&](auto const &eval) { return eval->matches(root, node); });
}
crawler::And::And(const std::vector<Evaluator *> &evalutors)
    : CombiningEvaluator(evalutors) {}

bool crawler::Or::matches(const crawler::Node &root,
                          const crawler::Node &node) {
  return std::any_of(
      this->evalutors.cbegin(), this->evalutors.cend(),
      [&](auto const &eval) { return eval->matches(root, node); });
}
crawler::Or::Or(const std::vector<Evaluator *> &evalutors)
    : CombiningEvaluator(evalutors) {}

crawler::Parent::Parent(std::shared_ptr<Evaluator *> _eval)
    : StructuralEvaluator(_eval){};

bool crawler::Parent::matches(const crawler::Node &root,
                              const crawler::Node &node) {
  Evaluator *eval = *this->evaluator;
  const std::shared_ptr<crawler::Node> parentPtr = node.getParent();
  if (parentPtr == nullptr) {
    return false;
  }
  auto parent = *parentPtr;
  while (true) {
    if (eval->matches(root, parent)) {
      return true;
    }
    if (parent.getParent() == nullptr) {
      return false;
    }
    parent = *parent.getParent();
  }
  return false;
}

crawler::ImmediateParent::ImmediateParent(std::shared_ptr<Evaluator *> _eval)
    : StructuralEvaluator(_eval){};

bool crawler::ImmediateParent::matches(const crawler::Node &root,
                                       const crawler::Node &node) {
  const std::shared_ptr<crawler::Node>& parentPtr = node.getParent();
  if (parentPtr == nullptr) {
    return false;
  }
  Evaluator *eval = *this->evaluator;
  return eval->matches(root, *parentPtr);
}
