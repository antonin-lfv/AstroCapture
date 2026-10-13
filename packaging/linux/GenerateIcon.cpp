#include <QCoreApplication>
#include <QImage>
#include <QSize>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 3) return 1;
    const QImage source(QString::fromLocal8Bit(argv[1]));
    if (source.isNull()) return 2;
    const auto icon = source.scaled(QSize(256, 256), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return icon.save(QString::fromLocal8Bit(argv[2]), "PNG") ? 0 : 3;
}
