#pragma once

#include <QAbstractTableModel>

namespace pmx {

class PmxDocumentController;

class PmxInfoModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum PropertyRow {
        RowModelNameJp = 0,
        RowModelNameEn,
        RowCommentJp,
        RowCommentEn,
        RowVersion,
        RowTextEncoding,
        RowAdditionalUVs,
        RowVertexIndexSize,
        RowTextureIndexSize,
        RowMaterialIndexSize,
        RowBoneIndexSize,
        RowMorphIndexSize,
        RowRigidBodyIndexSize,
        RowCount
    };
    Q_ENUM(PropertyRow)

    explicit PmxInfoModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxInfoModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void resetModel();

public slots:
    void onDisplayLanguageChanged();

private:
    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
