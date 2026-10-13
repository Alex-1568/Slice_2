#include "generators/identity/generators/ProfileMetadataGenerator.hpp"

#include "generators/identity/common/IdentityUtilities.hpp"

#include <utility>

namespace account_recommendations::generators::identity
{
namespace
{
const domain::ConfigurationSchema& schema()
{
    static const domain::ConfigurationSchema value = [] {
        QList<domain::FieldSpec> fields;
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("locale"), QStringLiteral("Locale"),
            builtInNameLocales(), QStringLiteral("en-US"), QStringLiteral("Profile")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("countryCode"), QStringLiteral("Country code"),
            {QStringLiteral("US"), QStringLiteral("GB"), QStringLiteral("ID"), QStringLiteral("ZZ")},
            QStringLiteral("US"), QStringLiteral("Profile")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("timezone"), QStringLiteral("Timezone"),
            {QStringLiteral("UTC"), QStringLiteral("America/New_York"), QStringLiteral("America/Los_Angeles"),
             QStringLiteral("Europe/London"), QStringLiteral("Asia/Jakarta"), QStringLiteral("Asia/Singapore")},
            QStringLiteral("UTC"), QStringLiteral("Profile")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("department"), QStringLiteral("Department"),
            {QStringLiteral("random"), QStringLiteral("Engineering"), QStringLiteral("Design"),
             QStringLiteral("Product"), QStringLiteral("Support"), QStringLiteral("Operations"),
             QStringLiteral("Research"), QStringLiteral("Marketing")}, QStringLiteral("random"), QStringLiteral("Work")));
        fields.append(domain::FieldSpec::enumeration(QStringLiteral("jobTitle"), QStringLiteral("Job title"),
            {QStringLiteral("random"), QStringLiteral("Software Engineer"), QStringLiteral("Product Designer"),
             QStringLiteral("Data Analyst"), QStringLiteral("Project Coordinator"),
             QStringLiteral("Support Specialist"), QStringLiteral("QA Engineer"),
             QStringLiteral("Research Associate")}, QStringLiteral("random"), QStringLiteral("Work")));
        fields.append(domain::FieldSpec::boolean(QStringLiteral("includeTag"), QStringLiteral("Include tag"),
                                                true, QStringLiteral("Profile")));
        return domain::ConfigurationSchema(std::move(fields));
    }();
    return value;
}

const QStringList& departments()
{
    static const QStringList values{QStringLiteral("Engineering"), QStringLiteral("Design"),
        QStringLiteral("Product"), QStringLiteral("Support"), QStringLiteral("Operations"),
        QStringLiteral("Research"), QStringLiteral("Marketing")};
    return values;
}
const QStringList& jobTitles()
{
    static const QStringList values{QStringLiteral("Software Engineer"), QStringLiteral("Product Designer"),
        QStringLiteral("Data Analyst"), QStringLiteral("Project Coordinator"),
        QStringLiteral("Support Specialist"), QStringLiteral("QA Engineer"),
        QStringLiteral("Research Associate")};
    return values;
}
const QStringList& tags()
{
    static const QStringList values{QStringLiteral("demo"), QStringLiteral("synthetic"),
        QStringLiteral("sample"), QStringLiteral("internal-test"), QStringLiteral("prototype")};
    return values;
}
} // namespace

ProfileMetadataGenerator::ProfileMetadataGenerator(std::shared_ptr<const INameDataProvider> provider)
    : m_provider(provider ? std::move(provider) : defaultNameDataProvider())
{
}

domain::GeneratorMetadata ProfileMetadataGenerator::metadata() const
{
    domain::GeneratorMetadata md;
    md.id = QStringLiteral("profile");
    md.displayName = QStringLiteral("Profile Metadata");
    md.description = QStringLiteral("Creates flat synthetic profile metadata for fixtures, demos, and tests; no real-person lookup is performed.");
    md.category = QStringLiteral("Identity");
    md.version = QStringLiteral("1.2.0");
    md.tags = {QStringLiteral("identity"), QStringLiteral("profile"), QStringLiteral("synthetic"), QStringLiteral("offline")};
    md.supportsBatch = true;
    md.supportsPresets = true;
    md.supportsUniqueness = true;
    md.configurationSchema = schema();
    md.outputFields = {
        {QStringLiteral("firstName"), QStringLiteral("First name"), false},
        {QStringLiteral("lastName"), QStringLiteral("Last name"), false},
        {QStringLiteral("displayName"), QStringLiteral("Display name"), false},
        {QStringLiteral("username"), QStringLiteral("Username"), false},
        {QStringLiteral("locale"), QStringLiteral("Locale"), false},
        {QStringLiteral("countryCode"), QStringLiteral("Country code"), false},
        {QStringLiteral("timezone"), QStringLiteral("Timezone"), false},
        {QStringLiteral("language"), QStringLiteral("Language"), false},
        {QStringLiteral("department"), QStringLiteral("Department"), false},
        {QStringLiteral("jobTitle"), QStringLiteral("Job title"), false},
        {QStringLiteral("tag"), QStringLiteral("Tag"), false},
    };
    return md;
}

common::ValidationResult ProfileMetadataGenerator::validateSpecific(
    const QJsonObject& parameters, const common::ApplicationLimits&) const
{
    common::ValidationResult issues;
    const QString selectedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto selectedDataset = m_provider
        ? m_provider->dataset(selectedLocale)
        : std::optional<NameDataset>{};
    if (!selectedDataset || selectedDataset->locale != selectedLocale
        || !isUsableNameDataset(*selectedDataset))
        issues.addError(QStringLiteral("locale"), QStringLiteral("unsupported_locale"),
                        QStringLiteral("The selected local name dataset is unavailable or invalid."));
    return issues;
}

common::Result<domain::GeneratedRecord, domain::GenerationError>
ProfileMetadataGenerator::generateRecord(const QJsonObject& parameters, const domain::GenerationContext& context) const
{
    using R = common::Result<domain::GeneratedRecord, domain::GenerationError>;
    const QString requestedLocale = parameters.value(QStringLiteral("locale")).toString();
    const auto dataset = m_provider->dataset(requestedLocale);
    if (!dataset || dataset->locale != requestedLocale || !isUsableNameDataset(*dataset))
        return R::failure(domain::GenerationError::constraintViolation(QStringLiteral("Name dataset unavailable.")));
    auto& random = context.random();
    const QString first = choose(dataset->firstNames, random);
    const QString last = choose(dataset->lastNames, random);
    const QString username = (first + QLatin1Char('.') + last + randomDigits(random, 3)).toLower();

    QString department = parameters.value(QStringLiteral("department")).toString();
    if (department == QLatin1String("random"))
        department = choose(departments(), random);
    QString jobTitle = parameters.value(QStringLiteral("jobTitle")).toString();
    if (jobTitle == QLatin1String("random"))
        jobTitle = choose(jobTitles(), random);

    domain::GeneratedRecord record;
    record.insert(QStringLiteral("firstName"), first);
    record.insert(QStringLiteral("lastName"), last);
    record.insert(QStringLiteral("displayName"), first + QLatin1Char(' ') + last);
    record.insert(QStringLiteral("username"), username);
    record.insert(QStringLiteral("locale"), dataset->locale);
    record.insert(QStringLiteral("countryCode"), parameters.value(QStringLiteral("countryCode")).toString());
    record.insert(QStringLiteral("timezone"), parameters.value(QStringLiteral("timezone")).toString());
    record.insert(QStringLiteral("language"), dataset->language);
    record.insert(QStringLiteral("department"), department);
    record.insert(QStringLiteral("jobTitle"), jobTitle);
    record.insert(QStringLiteral("tag"), parameters.value(QStringLiteral("includeTag")).toBool()
        ? choose(tags(), random) : QString());
    return R::success(std::move(record));
}

QString ProfileMetadataGenerator::uniqueKey(const domain::GeneratedRecord& record) const
{
    return record.value(QStringLiteral("username")).toString().toCaseFolded();
}

} // namespace account_recommendations::generators::identity
