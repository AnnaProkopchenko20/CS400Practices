#pragma once

struct InitNode : Node {
    std::vector<std::unique_ptr<Node>> values; // expression or other init node

};
