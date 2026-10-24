#include "models/PmxSelectionManager.h"

#include <algorithm>

namespace pmx {

PmxSelectionManager::PmxSelectionManager(QObject* parent)
    : QObject(parent)
{
}

QSet<int>& PmxSelectionManager::setForType(SelectionType type) {
    switch (type) {
    case SelectionType::Vertex:    return m_vertices;
    case SelectionType::Material:  return m_materials;
    case SelectionType::Bone:      return m_bones;
    case SelectionType::Morph:     return m_morphs;
    case SelectionType::RigidBody: return m_rigidBodies;
    case SelectionType::Joint:     return m_joints;
    }
    return m_vertices;
}

const QSet<int>& PmxSelectionManager::setForType(SelectionType type) const {
    switch (type) {
    case SelectionType::Vertex:    return m_vertices;
    case SelectionType::Material:  return m_materials;
    case SelectionType::Bone:      return m_bones;
    case SelectionType::Morph:     return m_morphs;
    case SelectionType::RigidBody: return m_rigidBodies;
    case SelectionType::Joint:     return m_joints;
    }
    return m_vertices;
}

QSet<int> PmxSelectionManager::selectedIndices(SelectionType type) const {
    return setForType(type);
}

QList<int> PmxSelectionManager::selectedIndicesList(SelectionType type) const {
    const auto& set = setForType(type);
    QList<int> list(set.begin(), set.end());
    std::sort(list.begin(), list.end());
    return list;
}

bool PmxSelectionManager::isSelected(SelectionType type, int index) const {
    return setForType(type).contains(index);
}

int PmxSelectionManager::selectionCount(SelectionType type) const {
    return setForType(type).size();
}

bool PmxSelectionManager::hasSelection(SelectionType type) const {
    return !setForType(type).isEmpty();
}

void PmxSelectionManager::setSelection(SelectionType type, const QSet<int>& indices) {
    auto& set = setForType(type);
    if (set == indices) {
        return;
    }
    set = indices;
    emit selectionChanged(type);
    emit anySelectionChanged();
}

void PmxSelectionManager::setSelection(SelectionType type, const QList<int>& indices) {
    QSet<int> newSet(indices.begin(), indices.end());
    setSelection(type, newSet);
}

void PmxSelectionManager::select(SelectionType type, int index) {
    auto& set = setForType(type);
    if (!set.contains(index)) {
        set.insert(index);
        emit selectionChanged(type);
        emit anySelectionChanged();
    }
}

void PmxSelectionManager::deselect(SelectionType type, int index) {
    auto& set = setForType(type);
    if (set.remove(index)) {
        emit selectionChanged(type);
        emit anySelectionChanged();
    }
}

void PmxSelectionManager::toggle(SelectionType type, int index) {
    auto& set = setForType(type);
    if (set.contains(index)) {
        set.remove(index);
    } else {
        set.insert(index);
    }
    emit selectionChanged(type);
    emit anySelectionChanged();
}

void PmxSelectionManager::clearSelection(SelectionType type) {
    auto& set = setForType(type);
    if (!set.isEmpty()) {
        set.clear();
        emit selectionChanged(type);
        emit anySelectionChanged();
    }
}

void PmxSelectionManager::clearAll() {
    bool hadAny = !m_vertices.isEmpty() || !m_materials.isEmpty() ||
                  !m_bones.isEmpty() || !m_morphs.isEmpty() ||
                  !m_rigidBodies.isEmpty() || !m_joints.isEmpty();
    if (hadAny) {
        m_vertices.clear();
        m_materials.clear();
        m_bones.clear();
        m_morphs.clear();
        m_rigidBodies.clear();
        m_joints.clear();
        emit selectionChanged(SelectionType::Vertex);
        emit selectionChanged(SelectionType::Material);
        emit selectionChanged(SelectionType::Bone);
        emit selectionChanged(SelectionType::Morph);
        emit selectionChanged(SelectionType::RigidBody);
        emit selectionChanged(SelectionType::Joint);
        emit anySelectionChanged();
    }
}

} // namespace pmx
