
#ifndef DOUBANCRAWLER_DOM_H
#define DOUBANCRAWLER_DOM_H

#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "strings.hpp"
namespace doubanCrawler {
using AttrMap = std::map<std::string, std::string>;

class ElementData {
   public:
    ElementData() = default;
    ElementData(std::string tagName, AttrMap attributes)
        : tagName(std::move(tagName)), attributes(std::move(attributes)) {}
    std::string id() {
        auto result = attributes.find("id");
        if (result != attributes.end()) {
            return result->second;
        } else {
            return std::string("");
        }
    }
    std::set<std::string> classes() {
        auto clazz = attributes.find("class");
        std::set<std::string> classElement;
        if (clazz != attributes.end()) {
            std::string result = clazz->second;
            doubanCrawler::split(result, classElement, '.');
        }
        return classElement;
    }
    [[nodiscard]] const std::string &getTagName() const {
        return tagName;
    }
    [[nodiscard]] const AttrMap &getAttributes() const {
        return attributes;
    }

   private:
    std::string tagName;
    AttrMap attributes;
};

typedef struct NodeType {
   public:
    bool isText() {
        return !text.empty();
    }
    bool isElementData() {
        return !isText();
    }
    ElementData element;
    std::string text;
} NodeData;

class Node {
   public:
    // data common to all nodes;
    explicit Node(const std::string &str) {
        nodeData.text = str;
        ElementData emptyElement;
        nodeData.element = emptyElement;
    };
    explicit Node(const std::string &_name, const AttrMap &_attrMap, std::vector<Node> _children) {
        children = std::move(_children);
        nodeData.text = "";
        ElementData elementData(_name, _attrMap);
        nodeData.element = elementData;
    }

    Node(const Node &node) {
        children = node.children;
        nodeData = node.nodeData;
    }
    [[nodiscard]] std::vector<Node> getChildren() const {
        return children;
    }

    [[nodiscard]] NodeData getNodeData() const {
        return nodeData;
    }

   private:
    // data common to all nodes;
    std::vector<Node> children;
    // data specific to each node type;
    NodeData nodeData;
};

using Elements = std::vector<Node>;

class Document : Node {
    using Node::Node;

   public:
    // breadth first search
    Elements getElementsByTag(const std::string &tagName) {
        Elements elementList;
        std::queue<Node> queue;
        Node root = *this;
        queue.push(root);
        while (!queue.empty()) {
            Node tempNode = queue.front();
            queue.pop();
            if (tempNode.getNodeData().isElementData() && tempNode.getNodeData().element.getTagName() == tagName) {
                elementList.emplace_back(tempNode);
            }
            if (!tempNode.getChildren().empty()) {
                for (auto const &childrenNode : tempNode.getChildren()) {
                    queue.push(childrenNode);
                }
            }
        }
        return elementList;
    }

   private:
    static Node *findFirstElementByTagName(const std::string &tagName, Node *node) {
        if (node->getNodeData().isElementData() && node->getNodeData().element.getTagName() == tagName) {
            return node;
        } else {
            for (auto &childrenNode : node->getChildren()) {
                Node *found = findFirstElementByTagName(tagName, &childrenNode);
                if (found != nullptr) {
                    return found;
                }
            }
        }
        return nullptr;
    }
};
}  // namespace doubanCrawler

#endif  // DOUBANCRAWLER_DOM_H
