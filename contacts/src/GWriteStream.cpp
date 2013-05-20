#include "GWriteStream.h"
#include "GContactCustomDetail.h"

#include <QXmlStreamWriter>
#include <QContactUrl>
#include <QContactAnniversary>
#include <QContact>
#include <QContactGuid>
#include <QContactName>
#include <QContactEmailAddress>
#include <QContactBirthday>
#include <QContactGender>
#include <QContactHobby>
#include <QContactNickname>
#include <QContactNote>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactAddress>
#include <QContactAvatar>
#include <QContactFamily>
#include <QContactTimestamp>
#include <QContactManager>

GWriteStream::GWriteStream () :
    mXmlWriter (&mXmlBuffer)
{
}

GWriteStream::~GWriteStream ()
{
}

void
GWriteStream::encodeAllContacts ()
{
    QContactManager mgr;
    QList<QContact> allContacts = mgr.contacts ();

    if (allContacts.size () <= 1)
        return;

    // QtContacts has a fictious first contact that has nothing
    // in it. Removing this unwanted element
    allContacts.removeFirst ();

    QList<QPair<QContact, GWriteStream::TRANSACTION_TYPE> > qContactPairList;
    QPair<QContact, GWriteStream::TRANSACTION_TYPE> contactPair;
    for (int i=0; i<allContacts.size (); i++)
    {
        contactPair.first = allContacts.at (i);
        contactPair.second = GWriteStream::ADD;
        qContactPairList.append (contactPair);
    }

    encodeContact (qContactPairList);
}

void
GWriteStream::encodeContacts (const QList<QContactLocalId> idList, TRANSACTION_TYPE type)
{
    QContactManager mgr;
    QList<QContact> contactList = mgr.contacts (idList);

    QList<QPair<QContact, GWriteStream::TRANSACTION_TYPE> > qContactPairList;
    QPair<QContact, GWriteStream::TRANSACTION_TYPE> contactPair;
    for (int i=0; i<contactList.size (); i++)
    {
        contactPair.first = contactList.at (i);
        contactPair.second = type;
        qContactPairList.append (contactPair);
    }

    encodeContact (qContactPairList);
}

QByteArray
GWriteStream::encodeContact (QList<QPair<QContact, TRANSACTION_TYPE> > qContactList)
{
    if (qContactList.size () <= 0)
        return mXmlBuffer;

    mXmlWriter.writeStartDocument ();
    // Encode as a batch
    bool batch = false;
    if (qContactList.size () > 1)
    {
        batch = true;
        startBatchFeed ();
    }

    for (int i=0; i<qContactList.size (); i++)
        if (!qContactList.at (i).first.isEmpty ())
            encodeContact (qContactList.at (i), batch);

    if (batch == true)
        endBatchFeed ();

    mXmlWriter.writeEndDocument ();

    return mXmlBuffer;
}

QByteArray
GWriteStream::encodedStream ()
{
    return mXmlBuffer;
}

QByteArray
GWriteStream::encodeContact(const QPair<QContact, TRANSACTION_TYPE> qContactPair,
                            const bool batch)
{
    QContact qContact = qContactPair.first;
    QList<QContactDetail> allDetails = qContact.details ();

    if (batch == true)
        mXmlWriter.writeStartElement ("atom:entry");
    else {
        mXmlWriter.writeStartElement ("atom:entry");
        mXmlWriter.writeAttribute ("xmlns:atom", "http://www.w3.org/2005/Atom");
        mXmlWriter.writeAttribute ("xmlns:gd", "http://schemas.google.com/g/2005");
        mXmlWriter.writeAttribute ("xmlns:gContact", "http://schemas.google.com/contact/2008");
    }

    TRANSACTION_TYPE updateType = qContactPair.second;
    // Etag encoding has to immediately succeed writeStartElement ("atom:entry"),
    // since etag is an attribute of this element
    encodeEtag (qContact);
    if (batch == true) encodeBatchTag (updateType);
    if (updateType == UPDATE)
    {
        encodeId (qContact);
        encodeUpdated (qContact);
    }

    if (updateType == DELETE)
    {
        mXmlWriter.writeEndElement ();
        return mXmlBuffer;
    }

    // Category is not required for deleted entries. So place it
    // here
    encodeCategory ();

    foreach (const QContactDetail& detail, allDetails)
    {
        if (detail.definitionName () == QContactName::DefinitionName)
            encodeName (detail);
        else if (detail.definitionName () == QContactPhoneNumber::DefinitionName)
            encodePhoneNumber (detail);
        else if (detail.definitionName () == QContactEmailAddress::DefinitionName)
            encodeEmail (detail);
        else if (detail.definitionName () == QContactAddress::DefinitionName)
            encodeAddress (detail);
        else if (detail.definitionName () == QContactUrl::DefinitionName)
            encodeUrl (detail);
        else if (detail.definitionName () == QContactBirthday::DefinitionName)
            encodeBirthDay (detail);
        else if (detail.definitionName () == QContactNote::DefinitionName)
            encodeNote (detail);
        else if (detail.definitionName () == QContactHobby::DefinitionName)
            encodeHobby (detail);
        else if (detail.definitionName () == QContactOrganization::DefinitionName)
            encodeOrganization (detail);
        else if (detail.definitionName () == QContactAvatar::DefinitionName)
            encodeAvatar (detail);
        else if (detail.definitionName () == QContactAnniversary::DefinitionName)
            encodeAnniversary (detail);
        else if (detail.definitionName () == QContactNickname::DefinitionName)
            encodeNickname (detail);
        else if (detail.definitionName () == QContactGender::DefinitionName)
            encodeGender (detail);
        else if (detail.definitionName () == QContactOnlineAccount::DefinitionName)
            encodeOnlineAccount(detail);
        else if (detail.definitionName () == QContactFamily::DefinitionName)
            encodeFamily (detail);
        // TODO: handle the custom detail fields. If sailfish UI
        // does not handle these contact details, then they
        // need not be pushed to the server
    }
    mXmlWriter.writeEndElement ();

    return mXmlBuffer;
}

void
GWriteStream::startBatchFeed ()
{
    mXmlWriter.writeStartElement ("atom:feed");
    mXmlWriter.writeAttribute ("xmlns:atom", "http://www.w3.org/2005/Atom");
    mXmlWriter.writeAttribute ("xmlns:gContact", "http://schemas.google.com/contact/2008");
    mXmlWriter.writeAttribute ("xmlns:gd", "http://schemas.google.com/g/2005");
    mXmlWriter.writeAttribute ("xmlns:batch", "http://schemas.google.com/gdata/batch");
}

void
GWriteStream::endBatchFeed ()
{
    mXmlWriter.writeEndElement ();
}

void
GWriteStream::encodeBatchTag (const TRANSACTION_TYPE type)
{
    if (type == ADD)
    {
        mXmlWriter.writeTextElement ("batch:id", "create");
        mXmlWriter.writeEmptyElement ("batch:operation");
        mXmlWriter.writeAttribute ("type", "insert");
    } else if (type == UPDATE)
    {
        mXmlWriter.writeTextElement ("batch:id", "update");
        mXmlWriter.writeEmptyElement ("batch:operation");
        mXmlWriter.writeAttribute ("type", "update");
    } else if (type == DELETE)
    {
        mXmlWriter.writeTextElement ("batch:id", "delete");
        mXmlWriter.writeEmptyElement ("batch:operation");
        mXmlWriter.writeAttribute ("type", "delete");
    }
}

void
GWriteStream::encodeId (const QContact qContact)
{
    QString guid = qContact.detail (
        QContactGuid::DefinitionName).value (QContactGuid::FieldGuid);

    if (!guid.isNull ())
        mXmlWriter.writeTextElement ("id", "http://www.google.com/m8/feeds/contacts/default/base/" + guid);
}

void
GWriteStream::encodeUpdated (const QContact qContact)
{
    QString updated = qContact.detail (
        QContactTimestamp::DefinitionName).value (QContactTimestamp::FieldModificationTimestamp);

    if (!updated.isNull ())
        mXmlWriter.writeTextElement ("updated", updated);
}

void
GWriteStream::encodeEtag (const QContact qContact)
{
    QString etag = qContact.detail (
        GContactCustomDetail::DefinitionName).value (GContactCustomDetail::FieldGContactETag);

    if (!etag.isNull ())
        mXmlWriter.writeAttribute ("gd:etag", etag);
}

void
GWriteStream::encodeCategory ()
{
    mXmlWriter.writeEmptyElement ("atom:category");
    mXmlWriter.writeAttribute ("schema", "http://schemas.google.com/g/2005#kind");
    mXmlWriter.writeAttribute ("term", "http://schemas.google.com/contact/2008#contact");
}

void
GWriteStream::encodeLink ()
{
}

void
GWriteStream::encodeName (const QContactDetail& detail)
{
    QContactName contactName = static_cast<QContactName>(detail);
    mXmlWriter.writeStartElement ("gd:name");

    if (!contactName.firstName ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:givenName", contactName.firstName ());
    if (!contactName.middleName().isEmpty())
        mXmlWriter.writeTextElement ("gd:additionalName", contactName.middleName ());
    if (!contactName.lastName().isEmpty())
        mXmlWriter.writeTextElement ("gd:familyName", contactName.lastName ());
    if (!contactName.prefix().isEmpty())
        mXmlWriter.writeTextElement ("gd:namePrefix", contactName.prefix ());
    if (!contactName.suffix().isEmpty())
        mXmlWriter.writeTextElement ("gd:nameSuffix", contactName.suffix ());

    mXmlWriter.writeEndElement ();
}

/*!
 * Encode Phone Number Field Information into the Google contact XML Document
 */
void
GWriteStream::encodePhoneNumber (const QContactDetail& detail)
{
    QContactPhoneNumber phoneNumber = static_cast<QContactPhoneNumber>(detail);
    QStringList subTypes = phoneNumber.subTypes();
    if (phoneNumber.number ().isEmpty ())
        return;

    mXmlWriter.writeStartElement ("gd:phoneNumber");

    QString rel = "http://schemas.google.com/g/2005#";
    // TODO: Handle subtype encoding properly
    if (subTypes.contains(QContactPhoneNumber::SubTypeAssistant))
        mXmlWriter.writeAttribute ("rel", rel + "assistant");
    else
        mXmlWriter.writeAttribute ("rel", rel + "mobile");

    mXmlWriter.writeCharacters (phoneNumber.number());
    mXmlWriter.writeEndElement ();
}

/*!
 * Encode Email Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeEmail (const QContactDetail& detail)
{
    QContactEmailAddress emailAddress = static_cast<QContactEmailAddress>(detail);
    if (emailAddress.emailAddress ().isEmpty ())
        return;

    // TODO: Handle 'rel' attribute, which is subtype
    mXmlWriter.writeEmptyElement ("gd:email");
    mXmlWriter.writeAttribute ("rel", "http://schemas.google.com/g/2005#other");
    mXmlWriter.writeAttribute ("address", emailAddress.emailAddress ());
}

/*!
 * Encode Address Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeAddress (const QContactDetail& detail)
{
    QContactAddress address = static_cast<QContactAddress>(detail);

    mXmlWriter.writeStartElement ("gd:structuredPostalAddress");
    mXmlWriter.writeAttribute ("rel", "http://schemas.google.com/g/2005#other");

    if (!address.street ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:street", address.street ());
    if (!address.locality ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:neighborhood", address.locality ());
    if (!address.postOfficeBox ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:pobox", address.postOfficeBox ());
    if (!address.region ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:region", address.region ());
    if (!address.postcode ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:postcode", address.postcode ());
    if (!address.country ().isEmpty ())
        mXmlWriter.writeTextElement ("gd:country", address.country ());

    mXmlWriter.writeEndElement ();
}

/*!
 * Encode URL Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeUrl (const QContactDetail& detail)
{
    QContactUrl contactUrl = static_cast<QContactUrl>(detail);
    if (contactUrl.url ().isEmpty ())
        return;
    mXmlWriter.writeEmptyElement ("gContact:website");
    mXmlWriter.writeAttribute ("rel", "home-page");
    mXmlWriter.writeAttribute ("href", contactUrl.url ());
}

/*!
 * Encode BirthDay Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeBirthDay (const QContactDetail& detail)
{
    QContactBirthday bday = static_cast<QContactBirthday>(detail);
    if (bday.date ().isNull ())
        return;

    QVariant variant = bday.variantValue(QContactBirthday::FieldBirthday);
    QString value = variant.toDate().toString(Qt::ISODate);

    mXmlWriter.writeEmptyElement ("gContact:birthday");
    mXmlWriter.writeAttribute ("when", value);
}

/*!
 * Encode Comment i.e. Note Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeNote (const QContactDetail& detail)
{
    QContactNote contactNote = static_cast<QContactNote>(detail);
    if (contactNote.note ().isEmpty ())
        return;

    mXmlWriter.writeStartElement ("atom:content");
    mXmlWriter.writeAttribute ("type", "text");
    mXmlWriter.writeCharacters (contactNote.note ());
    mXmlWriter.writeEndElement ();
}

/*!
 * Encode Hobby i.e. Hobby Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeHobby (const QContactDetail& detail)
{
    QContactHobby contactHobby = static_cast<QContactHobby>(detail);
    if (contactHobby.hobby ().isEmpty ())
        return;
    mXmlWriter.writeTextElement ("gContact:hobby", contactHobby.hobby ());
}

/*!
 * Encode Geo Prpoperties Field Information into the Google contact XML Document
 */
void
GWriteStream::encodeGeoLocation (const QContactDetail& detail)
{
    /* TODO: Need to check how to handle GeoRSS geo location
    QContactGeoLocation geoLocation = static_cast<QContactGeoLocation>(detail);
    property.setValue(QStringList() << QString::number(geoLocation.latitude())
                      << QString::number(geoLocation.longitude()));
    */
}

/*!
 * Encode organization properties to the versit document
 */
void
GWriteStream::encodeOrganization (const QContactDetail& detail)
{
    QContactOrganization organization = static_cast<QContactOrganization>(detail);
    mXmlWriter.writeStartElement ("gd:organization");
    // FIXME: The organization type should be obtained from DB
    mXmlWriter.writeAttribute ("rel", "http://schemas.google.com/g/2005#work");
    if (organization.title().length () > 0)
        mXmlWriter.writeTextElement ("gd:orgTitle", organization.title ());
    if (organization.name().length() > 0 )
        mXmlWriter.writeTextElement ("gd:orgName", organization.name ());
    if (organization.department ().length () > 0)
    {
        QString departmets;
        foreach (const QString dept, organization.department ())
            departmets.append (dept);

        mXmlWriter.writeTextElement ("gd:orgDepartment", departmets);
    }

    mXmlWriter.writeEndElement ();
}

/*!
 * Encode avatar URIs into the Google contact XML Document
 */
void
GWriteStream::encodeAvatar (const QContactDetail &detail)
{
    /*
    property.setName(QLatin1String("PHOTO"));
    QContactAvatar contactAvatar = static_cast<QContactAvatar>(detail);
    QUrl imageUrl(contactAvatar.imageUrl());
    // XXX: fix up this mess: checking the scheme here and in encodeContentFromFile,
    // organisation logo and ringtone are QStrings but avatar is a QUrl
    if (!imageUrl.scheme().isEmpty()
            && !imageUrl.host().isEmpty()
            && imageUrl.scheme() != QLatin1String("file")) {
        property.insertParameter(QLatin1String("VALUE"), QLatin1String("URL"));
        property.setValue(imageUrl.toString());
        *generatedProperties << property;
        *processedFields << QContactAvatar::FieldImageUrl;
    } else {
        if (encodeContentFromFile(contactAvatar.imageUrl().toLocalFile(), property)) {
            *generatedProperties << property;
            *processedFields << QContactAvatar::FieldImageUrl;
        }
    }
    */
}

/*!
 * Encode gender property information into Google contact XML Document
 */
void
GWriteStream::encodeGender (const QContactDetail &detail)
{
    QContactGender gender = static_cast<QContactGender>(detail);
    if (gender.gender ().isEmpty ())
        return;
    mXmlWriter.writeEmptyElement ("gContact:gender");
    mXmlWriter.writeAttribute ("value", gender.gender ().toLower ());
}

/*!
 * Encodes nickname property information into the Google contact XML Document
 */
void
GWriteStream::encodeNickname (const QContactDetail &detail)
{
    QContactNickname nicknameDetail = static_cast<QContactNickname>(detail);
    if (nicknameDetail.nickname ().isEmpty ())
        return;
    mXmlWriter.writeTextElement ("gContact:nickname", nicknameDetail.nickname ());
}

/*!
 * Encode anniversary information into Google contact XML Document
 */
void
GWriteStream::encodeAnniversary (const QContactDetail &detail)
{
    QContactAnniversary anniversary = static_cast<QContactAnniversary>(detail);
    if (anniversary.event ().isEmpty ())
        return;
    mXmlWriter.writeStartElement ("gContact:event");
    mXmlWriter.writeAttribute ("rel", "anniversary");
    mXmlWriter.writeEmptyElement ("gd:when");
    mXmlWriter.writeAttribute ("startTime", anniversary.originalDate ().toString (Qt::ISODate));
    mXmlWriter.writeEndElement ();
}

/*!
 * Encode online account information into the Google contact XML Document
 */
void
GWriteStream::encodeOnlineAccount (const QContactDetail &detail)
{
    QContactOnlineAccount onlineAccount = static_cast<QContactOnlineAccount>(detail);
    if (onlineAccount.accountUri ().isEmpty ())
        return;

    QStringList subTypes = onlineAccount.subTypes();
    QString protocol = onlineAccount.protocol();

    QString propertyName;

    if (protocol == QContactOnlineAccount::ProtocolJabber) {
        propertyName = "JABBER";
    } else if (protocol == QContactOnlineAccount::ProtocolAim) {
        propertyName = "AIM";
    } else if (protocol == QContactOnlineAccount::ProtocolIcq) {
        propertyName = "ICQ";
    } else if (protocol == QContactOnlineAccount::ProtocolMsn) {
        propertyName = "MSN";
    } else if (protocol == QContactOnlineAccount::ProtocolQq) {
        propertyName = "QQ";
    } else if (protocol == QContactOnlineAccount::ProtocolYahoo) {
        propertyName = "YAHOO";
    } else if (protocol == QContactOnlineAccount::ProtocolSkype) {
        propertyName = "SKYPE";
    } else
        propertyName = protocol;

    mXmlWriter.writeEmptyElement ("gd:im");
    mXmlWriter.writeAttribute ("protocol", "http://schemas.google.com/g/2005#" + propertyName);
    mXmlWriter.writeAttribute ("address", onlineAccount.accountUri ());
}

/*!
 * Encode family property if its supported in Google contact XML Document
 */
void
GWriteStream::encodeFamily (const QContactDetail &detail)
{
    QContactFamily family = static_cast<QContactFamily>(detail);

    if (family.spouse().length () > 0) {
        mXmlWriter.writeEmptyElement ("gContact:relation");
        mXmlWriter.writeAttribute ("rel", "spouse");
        mXmlWriter.writeCharacters (family.spouse ());
    }

    foreach (const QString member, family.children ())
    {
        mXmlWriter.writeEmptyElement ("gContact:relation");
        mXmlWriter.writeAttribute ("rel", "child");
        mXmlWriter.writeCharacters (member);
    }
}
