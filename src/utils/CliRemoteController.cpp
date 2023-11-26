#include "CliRemoteController.h"

#include "Log.h"
#include "config/DspConfig.h"
#include "data/PresetManager.h"
#include "data/model/PresetListModel.h"

#include <QVariant>

CliRemoteController::CliRemoteController(QObject *parent)
    : QObject{parent}
{
    DspConfig::instance().load();

    const QMetaObject* meta = metaObject();
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        _options.append(meta->property(i).read(this).value<QCommandLineOption*>());
    }
}

CliRemoteController::~CliRemoteController()
{
    for(auto* opt : _options)
        delete opt;
}

void CliRemoteController::attachClient()
{
    if(service == nullptr) {
        service = new me::timschneeberger::jdsp4linux::Service("me.timschneeberger.jdsp4linux", "/jdsp4linux/service",
                                                               QDBusConnection::sessionBus(), this);
    }
}

void CliRemoteController::registerOptions(QCommandLineParser &parser) const
{
    for(auto* opt : _options)
        parser.addOption(*opt);
}

bool CliRemoteController::isAnyArgumentSet(QCommandLineParser &parser) const
{
    for(auto* opt : _options) {
        if(parser.isSet(*opt))
            return true;
    }
    return false;
}

bool CliRemoteController::execute(QCommandLineParser &parser)
{
    attachClient();

    const QMetaObject* meta = metaObject();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        QMetaMethod method = meta->method(i);

        QMetaProperty prop = meta->property(metaObject()->indexOfProperty(QString::fromLocal8Bit(method.name()).prepend('_').toLocal8Bit().constData()));
        QCommandLineOption* opt = prop.read(this).value<QCommandLineOption*>();

        if(!prop.isValid()) {
            Log::error(QString("Bug: Method '%1' has no corresponding property assigned").arg(QString::fromLocal8Bit(method.name())));
            continue;
        }

        bool result = false;
        if(parser.isSet(*opt)) {
            // Argument without parameter
            if(method.parameterCount() < 1) {
                method.invoke(this, Q_RETURN_ARG(bool, result));
                return result;
            }
            // Argument with parameter
            else if(method.parameterCount() == 1 && method.parameterType(0) == QMetaType::QString) {
                QString inputValue = parser.value(*opt);
                if(!inputValue.isEmpty()) {
                    method.invoke(this, Q_RETURN_ARG(bool, result), Q_ARG(QString, inputValue));
                    return result;
                }
            }
            else {
                Log::error(QString("Bug: Invokable method '%1' has wrong function signature for property '%2'").arg(QString::fromLocal8Bit(method.name())).arg(prop.name()));
                continue;
            }
        }
    }

    return true;
}

template<typename T>
std::optional<T> handleReply(QDBusPendingReply<T>& reply) {
    reply.waitForFinished();

    if(reply.isError()) {
        Log::error(reply.error().message());
        return {};
    }
    else
        return { reply.value() };
}

bool handleVoidReply(QDBusPendingReply<>& reply) {
    reply.waitForFinished();

    if(reply.isError()) {
        Log::error(reply.error().message());;
    }
    return reply.isError();
}

bool validateKey(const QString& keyString, DspConfig::Key& resolvedKey) {
    QMetaEnum meta = QMetaEnum::fromType<DspConfig::Key>();
    bool valid = false;
    resolvedKey = static_cast<DspConfig::Key>(meta.keyToValue(keyString.toLocal8Bit().constData(), &valid));
    if(!valid) {
        Log::error("Configuration key does not exist");
        return false;
    }
    return true;
}

bool CliRemoteController::isConnected() const
{
    Log::console(service->isValid() ? "true" : "false", true);
    return service->isValid();
}

bool CliRemoteController::set(const QString &keyValue) const
{
    QPair<QString, QVariant> out;
    bool validLine = ConfigIO::readLine(keyValue, out);
    if(!validLine) {
        Log::error("Syntax error. Please use this format: --set key=value or --set \"key=value with spaces\".");
        return false;
    }

    QString key = out.first;
    QVariant value = out.second;

    if(!checkConnectionAndLog()) {
        DspConfig::Key resolvedKey;
        bool valid = validateKey(key, resolvedKey);
        if(!valid)
            return false;

        DspConfig::instance().set(resolvedKey, value);
        DspConfig::instance().save();
        return true;
    }

    auto reply = service->setAndCommit(key, QDBusVariant(value));
    return handleVoidReply(reply);
}

bool CliRemoteController::get(const QString &key) const
{
    std::optional<QString> result;
    if(!checkConnectionAndLog()) {
        DspConfig::Key resolvedKey;
        bool valid = validateKey(key, resolvedKey);
        if(!valid)
            result = std::nullopt;

        result = DspConfig::instance().get<QString>(resolvedKey, nullptr, false);
    }
    else {
        auto reply = service->get(key);
        auto received = handleReply<QString>(reply);
        result = received;
    }

    if(result.has_value()) {
        Log::console(result.value(), true);
    }

    return result.has_value();
}

bool CliRemoteController::getAll() const
{
    std::optional<QString> result;
    if(!checkConnectionAndLog()) {
        result = DspConfig::instance().serialize();
    }
    else {
        auto reply = service->getAll();
        auto received = handleReply<QString>(reply);
        result = received;
    }

    if(result.has_value()) {
        Log::console(result.value(), true);
    }

    return result.has_value();
}

bool CliRemoteController::listKeys() const
{
    QStringList keys;
    if(checkConnectionAndLog()) {
        auto reply = service->getKeys();
        keys = handleReply(reply).value_or(keys);
    }
    keys = keys.empty() ? DspConfig::getKeys() : keys;
    Log::console(keys.join("\n"), true);
    return !keys.empty();
}

bool CliRemoteController::listPresets() const
{
    auto *model = PresetManager::instance().presetModel();
    model->rescan();
    for(const auto& name : model->getList())
        Log::console(name, true);
    return model->rowCount() > 0;
}

bool CliRemoteController::loadPreset(const QString &name) const
{
    if(checkConnectionAndLog()) {
        auto reply = service->loadPreset(name);
        return handleVoidReply(reply);
    }

    bool success = PresetManager::instance().load(name);
    if(!success)
        Log::error("Preset does not exist");
    return success;
}

bool CliRemoteController::savePreset(const QString &name) const
{
    if(checkConnectionAndLog()) {
        auto reply = service->savePreset(name);
        return handleVoidReply(reply);
    }

    PresetManager::instance().save(name);
    return true;
}

bool CliRemoteController::deletePreset(const QString &name) const
{
    if(checkConnectionAndLog()) {
        auto reply = service->deletePreset(name);
        return handleVoidReply(reply);
    }

    PresetManager::instance().remove(name);
    return true;
}

bool CliRemoteController::listPresetRules() const
{
#ifdef USE_PULSEAUDIO
    Log::error("This feature is only supported in the PipeWire version of JamesDSP");
    return false;
#endif

    QList<PresetRule> rules;
    if(checkConnectionAndLog()) {
        auto reply = service->getPresetRules();
        rules = handleReply(reply).value_or(rules);
    }

    rules = rules.empty() ? PresetManager::instance().rules().toList() : rules;
    for(const PresetRule& rule : rules) {
        Log::console(rule.deviceId + "=" + rule.preset, true);
    }
    return !rules.empty();
}

bool CliRemoteController::addPresetRule(const QString &keyValue) const
{
#ifdef USE_PULSEAUDIO
    Log::error("This feature is only supported in the PipeWire version of JamesDSP");
    return false;
#endif

    QStringList split = keyValue.split("=", Qt::SkipEmptyParts);
    if(split.count() < 2) {
        Log::warning("Invalid format. Expected input format: deviceId=presetName");
        return false;
    }

    QString id = split.at(0);
    QString preset = keyValue.mid(keyValue.indexOf('=') + 1);

    if(checkConnectionAndLog()) {
        auto reply = service->setPresetRule(id, id, preset);
        return handleVoidReply(reply);
    }

    PresetManager::instance().addRule(PresetRule(id, id, preset));
    return true;
}

bool CliRemoteController::deletePresetRule(const QString &deviceId) const
{
#ifdef USE_PULSEAUDIO
    Log::error("This feature is only supported in the PipeWire version of JamesDSP");
    return false;
#endif

    if(checkConnectionAndLog()) {
        auto reply = service->deletePresetRule(deviceId);
        return handleVoidReply(reply);
    }

    PresetManager::instance().removeRule(deviceId);
    return true;
}

bool CliRemoteController::listOutputDevices() const
{
    if(!requireConnectionAndLog())
        return false;

    QList<IOutputDevice> devices;
    auto reply = service->getOutputDevices();
    devices = handleReply(reply).value_or(devices);

    for(const IOutputDevice& device : devices) {
        Log::console(QString("Name: %1; Identifier: %2").arg(QString::fromStdString(device.description)).arg(QString::fromStdString(device.name)), true);
    }

    return true;
}

bool CliRemoteController::showStatus() const
{
    if(!requireConnectionAndLog())
        return false;

    Log::console("App version:\t" + service->appVersion(), true);
    Log::console("Core version:\t" + service->coreVersion(), true);
    Log::console("Sampling rate:\t" + service->samplingRate() + "Hz", true);
    Log::console("Audio format:\t" + service->audioFormat(), true);
    Log::console("Is processing:\t" + QString(service->isProcessing() ? "enabled" : "disabled"), true);
    return true;
}

// Helper to warn about failed connection
bool CliRemoteController::checkConnectionAndLog() const
{
    bool valid = service->isValid();
    if(!valid) Log::warning("JamesDSP service is offline; falling back to direct file access");
    return valid;
}

// Helper to alert about failed connection
bool CliRemoteController::requireConnectionAndLog() const
{
    bool valid = service->isValid();
    if(!valid) Log::error("JamesDSP service is offline. Please start it first and try again.");
    return valid;
}


