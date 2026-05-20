/****************************************************************************
 Copyright (c) 2019-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "UIModelProxy.h"
#include "2d/renderer/RenderEntity.h"
#include "core/assets/RenderingSubMesh.h"

namespace cc {
namespace {
bool isModelDrawInfo(const RenderDrawInfo* drawInfo) {
    return drawInfo != nullptr && drawInfo->getEnumDrawInfoType() == RenderDrawInfoType::MODEL;
}
} // namespace

UIModelProxy::UIModelProxy() {
    _device = Root::getInstance()->getDevice();
}

UIModelProxy::~UIModelProxy() {
    destroy();
}

void UIModelProxy::initModel(Node* node) {
    _model = Root::getInstance()->createModel<scene::Model>();
    _model->setNode(node);
    _model->setTransform(node);
    _node = node;
}

void UIModelProxy::activeSubModels() {
    if (_model == nullptr || _node == nullptr) return;
    auto* entity = static_cast<RenderEntity*>(_node->getUserData());
    if (entity == nullptr) return;

    auto drawInfoSize = entity->getRenderDrawInfosSize();
    auto subModelSize = _model->getSubModels().size();
    if (drawInfoSize > subModelSize) {
        for (size_t i = subModelSize; i < drawInfoSize; i++) {
            if (_model->getSubModels().size() <= i) {
                RenderDrawInfo* drawInfo = entity->getRenderDrawInfoAt(static_cast<uint32_t>(i));
                if (!isModelDrawInfo(drawInfo)) {
                    continue;
                }

                auto* vertexBuffer = _device->createBuffer({
                    gfx::BufferUsageBit::VERTEX | gfx::BufferUsageBit::TRANSFER_DST,
                    gfx::MemoryUsageBit::DEVICE,
                    65535 * _stride,
                    _stride,
                });
                auto* indexBuffer = _device->createBuffer({
                    gfx::BufferUsageBit::INDEX | gfx::BufferUsageBit::TRANSFER_DST,
                    gfx::MemoryUsageBit::DEVICE,
                    65535 * sizeof(uint16_t) * 2,
                    sizeof(uint16_t),
                });
                gfx::BufferList vbReference;
                vbReference.emplace_back(vertexBuffer);

                auto* renderMesh = ccnew RenderingSubMesh(vbReference, _attributes, _primitiveMode, indexBuffer);
                renderMesh->setSubMeshIdx(0);

                _model->initSubModel(static_cast<index_t>(i), renderMesh, drawInfo->getMaterial());
                _graphicsUseSubMeshes.emplace_back(renderMesh);
            }
        }
    }
}

void UIModelProxy::uploadData() {
    if (_model == nullptr || _node == nullptr) return;
    auto* entity = static_cast<RenderEntity*>(_node->getUserData());
    if (entity == nullptr) return;

    const auto drawInfoSize = entity->getRenderDrawInfosSize();
    const auto& subModelList = _model->getSubModels();
    RenderDrawInfo* firstModelDrawInfo = nullptr;
    for (size_t i = 0; i < drawInfoSize && i < subModelList.size(); i++) {
        auto* drawInfo = entity->getRenderDrawInfoAt(static_cast<uint32_t>(i));
        if (!isModelDrawInfo(drawInfo)) continue;
        if (firstModelDrawInfo == nullptr) {
            firstModelDrawInfo = drawInfo;
        }

        auto* ia = subModelList[i]->getInputAssembler();
        if (drawInfo->getVertexOffset() <= 0 || drawInfo->getIndexOffset() <= 0) continue;
        gfx::BufferList vBuffers = ia->getVertexBuffers();
        if (!vBuffers.empty()) {
            auto size = drawInfo->getVertexOffset() * _stride;
            // if (size > vBuffers[0]->getSize()) {
            vBuffers[0]->resize(size);
            // }
            vBuffers[0]->update(drawInfo->getVDataBuffer()); // vdata
        }
        ia->setVertexCount(drawInfo->getVertexOffset()); // count

        gfx::Buffer* iBuffer = ia->getIndexBuffer();
        auto size = drawInfo->getIndexOffset() * 2;
        // if (size > iBuffer->getSize()) {
        iBuffer->resize(size);
        // }
        iBuffer->update(drawInfo->getIDataBuffer());   // idata
        ia->setIndexCount(drawInfo->getIndexOffset()); // indexCount
        // drawInfo->setModel(_model); // hack, render by model
    }

    if (firstModelDrawInfo != nullptr) {
        firstModelDrawInfo->setModel(_model);
    }
}

void UIModelProxy::destroy() {
    if (_model != nullptr) {
        Root::getInstance()->destroyModel(_model);
        _model = nullptr;
    }

    for (auto& subMesh : _graphicsUseSubMeshes) {
        subMesh->destroy();
        subMesh = nullptr;
    }
    _graphicsUseSubMeshes.clear();

    _models.clear();
}

void UIModelProxy::clear() {
    if (_model == nullptr) {
        return;
    }
    const auto& subModels = _model->getSubModels();
    for (const auto &subModel : subModels) {
        auto *ia = subModel->getInputAssembler();
        ia->setVertexCount(0);
        ia->setIndexCount(0);
    }
}

// for ui model
void UIModelProxy::updateModels(scene::Model* model) {
    _models.emplace_back(model);
}

void UIModelProxy::attachDrawInfo() {
    if (_node == nullptr) return;
    auto* entity = static_cast<RenderEntity*>(_node->getUserData());
    if (entity == nullptr) return;

    auto drawInfoSize = entity->getRenderDrawInfosSize();
    if (drawInfoSize != _models.size()) return;
    for (size_t i = 0; i < drawInfoSize; i++) {
        auto* drawInfo = entity->getRenderDrawInfoAt(static_cast<uint32_t>(i));
        if (isModelDrawInfo(drawInfo)) {
            drawInfo->setModel(_models[i]);
        }
    }
}

void UIModelProxy::attachNode(Node* node) {
    _node = node;
}

void UIModelProxy::clearModels() {
    _models.clear();
}

} // namespace cc
