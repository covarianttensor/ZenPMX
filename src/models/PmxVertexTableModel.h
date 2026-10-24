#pragma once

#include "core/PmxTypes.h"

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace pmx {

class PmxDocumentController;

class PmxVertexTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColIndex = 0,
        ColPosX,
        ColPosY,
        ColPosZ,
        ColNormX,
        ColNormY,
        ColNormZ,
        ColUvU,
        ColUvV,
        ColWeightType,
        ColBone0,
        ColBone1,
        ColBone2,
        ColBone3,
        ColWeight0,
        ColWeight1,
        ColWeight2,
        ColWeight3,
        ColEdgeScale,
        ColumnCount
    };
    Q_ENUM(Column)

    explicit PmxVertexTableModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxVertexTableModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Batch operations across multiple rows pushed as a single undoable command
    bool applyBatchOperation(const QList<int>& rowIndices, int column, const QString& opType, double operand);

    void resetModel();

public slots:
    void onDisplayLanguageChanged();

private:
    friend class PmxBatchOperationCommand;

    static double applyMathOp(double current, const QString& op, double operand);
    static QVariant getVertexFieldValue(const PmxVertex& v, int column, int row);
    static void applyValueToVertex(PmxVertex& v, int column, const QVariant& value);

    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
