// Checks that the plugin .so can be loaded the same way OpenRGB's PluginManager does.
#include <QCoreApplication>
#include <QJsonDocument>
#include <QPluginLoader>
#include <cstdio>
#include "OpenRGBPluginInterface.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QPluginLoader loader(argv[1]);
    QJsonObject md = loader.metaData();
    printf("metadata: %s\n", QJsonDocument(md.value("MetaData").toObject()).toJson(QJsonDocument::Compact).constData());
    QObject* inst = loader.instance();
    if(!inst) { printf("load error: %s\n", loader.errorString().toUtf8().constData()); return 1; }
    auto* p = qobject_cast<OpenRGBPluginInterface*>(inst);
    if(!p) { printf("qobject_cast failed\n"); return 1; }
    printf("api version: %u, name: %s\n", p->GetPluginAPIVersion(), p->GetPluginInfo().Name.c_str());
    return 0;
}
