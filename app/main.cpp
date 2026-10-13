#include "gui/MainWindow.hpp"

#include "domain/generator/GeneratorRegistry.hpp"
#include "generators/identity/registerIdentityGenerators.hpp"
#include "generators/password/PasswordGenerator.hpp"
#include "infrastructure/random/SecureRandomProvider.hpp"

#include <QApplication>
#include <QMessageBox>

#include <memory>

using namespace account_recommendations;

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AccountRecommendations"));
    QCoreApplication::setApplicationName(QStringLiteral("Account Recommendations"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    domain::GeneratorRegistry registry;
    if (const auto registered = registry.registerGenerator(
            std::make_unique<generators::password::PasswordGenerator>()); !registered) {
        QMessageBox::critical(nullptr, QObject::tr("Startup failed"), registered.error().message);
        return 1;
    }
    if (const auto registered = generators::identity::registerIdentityGenerators(registry); !registered) {
        QMessageBox::critical(nullptr, QObject::tr("Startup failed"), registered.error().message);
        return 1;
    }

    infrastructure::SecureRandomProvider random;
    if (const auto health = random.probe(); !health) {
        QMessageBox::critical(nullptr, QObject::tr("Secure randomness unavailable"),
                              QObject::tr("The operating-system random source is unavailable. The application will exit rather than use a weaker fallback.\n\n%1")
                                  .arg(health.error().description));
        return 2;
    }

    // Runtime loading is disabled initially. The user must select a library and confirm that
    // native plugin code runs with the application's full process privileges.
    infrastructure::RuntimePluginManager pluginManager(registry);
    gui::MainWindow window(registry, random, pluginManager);
    window.show();
    return application.exec();
}
