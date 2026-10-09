#pragma once
#include <QFileInfo>
namespace astro::app {
// A missing source still reserves its original path. Two missing files are
// not the same file merely because both canonical paths are empty.
inline bool sameFile(const QString& a,const QString& b) {
    if(a.isEmpty()||b.isEmpty())return false;
    const QFileInfo left(a),right(b);
    return left.absoluteFilePath()==right.absoluteFilePath()||(!left.canonicalFilePath().isEmpty()&&left.canonicalFilePath()==right.canonicalFilePath());
}
}
