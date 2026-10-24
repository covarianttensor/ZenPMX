#pragma once

#include <QList>
#include <QObject>
#include <QSet>

namespace pmx {

class PmxSelectionManager : public QObject {
    Q_OBJECT

public:
    enum class SelectionType {
        Vertex,
        Material,
        Bone,
        Morph,
        RigidBody,
        Joint
    };
    Q_ENUM(SelectionType)

    explicit PmxSelectionManager(QObject* parent = nullptr);
    ~PmxSelectionManager() override = default;

    // Queries
    QSet<int> selectedIndices(SelectionType type) const;
    QList<int> selectedIndicesList(SelectionType type) const;
    bool isSelected(SelectionType type, int index) const;
    int selectionCount(SelectionType type) const;
    bool hasSelection(SelectionType type) const;

    // Mutators
    void setSelection(SelectionType type, const QSet<int>& indices);
    void setSelection(SelectionType type, const QList<int>& indices);
    void select(SelectionType type, int index);
    void deselect(SelectionType type, int index);
    void toggle(SelectionType type, int index);
    void clearSelection(SelectionType type);
    void clearAll();

    // Convenience typed accessors
    QList<int> selectedVertices() const { return selectedIndicesList(SelectionType::Vertex); }
    QList<int> selectedMaterials() const { return selectedIndicesList(SelectionType::Material); }
    QList<int> selectedBones() const { return selectedIndicesList(SelectionType::Bone); }
    QList<int> selectedMorphs() const { return selectedIndicesList(SelectionType::Morph); }
    QList<int> selectedRigidBodies() const { return selectedIndicesList(SelectionType::RigidBody); }
    QList<int> selectedJoints() const { return selectedIndicesList(SelectionType::Joint); }

    void setSelectedVertices(const QList<int>& indices) { setSelection(SelectionType::Vertex, indices); }
    void setSelectedMaterials(const QList<int>& indices) { setSelection(SelectionType::Material, indices); }
    void setSelectedBones(const QList<int>& indices) { setSelection(SelectionType::Bone, indices); }
    void setSelectedMorphs(const QList<int>& indices) { setSelection(SelectionType::Morph, indices); }
    void setSelectedRigidBodies(const QList<int>& indices) { setSelection(SelectionType::RigidBody, indices); }
    void setSelectedJoints(const QList<int>& indices) { setSelection(SelectionType::Joint, indices); }

signals:
    void selectionChanged(SelectionType type);
    void anySelectionChanged();

private:
    QSet<int> m_vertices;
    QSet<int> m_materials;
    QSet<int> m_bones;
    QSet<int> m_morphs;
    QSet<int> m_rigidBodies;
    QSet<int> m_joints;

    QSet<int>& setForType(SelectionType type);
    const QSet<int>& setForType(SelectionType type) const;
};

} // namespace pmx
