/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QLOWENERGYCONTROLLERPRIVATE_P_H
#define QLOWENERGYCONTROLLERPRIVATE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#if defined(QT_OSX_BLUETOOTH) || defined(QT_IOS_BLUETOOTH)

#include <QtCore/qglobal.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QLowEnergyControllerPrivate : public QObject
{
public:
    // This class is required to make shared pointer machinery and
    // moc (== Obj-C syntax) happy on both OS X and iOS.
};

QT_END_NAMESPACE

#else

#include <qglobal.h>
#include <QtCore/QQueue>
#include <QtCore/QVector>
#include <QtBluetooth/qbluetooth.h>
#include <QtBluetooth/qlowenergycharacteristic.h>
#include "qlowenergycontroller.h"
#include "qlowenergyserviceprivate_p.h"

#if defined(QT_BLUEZ_BLUETOOTH) && !defined(QT_BLUEZ_NO_BTLE)
#include <QtBluetooth/QBluetoothSocket>
#elif defined(QT_ANDROID_BLUETOOTH)
#include <QtAndroidExtras/QAndroidJniObject>
#include "android/lowenergynotificationhub_p.h"
#endif

#include <functional>

QT_BEGIN_NAMESPACE

class QLowEnergyServiceData;

#if defined(QT_BLUEZ_BLUETOOTH) && !defined(QT_BLUEZ_NO_BTLE)
class HciManager;
class QSocketNotifier;
#elif defined(QT_ANDROID_BLUETOOTH)
class LowEnergyNotificationHub;
#endif

extern void registerQLowEnergyControllerMetaType();

typedef QMap<QBluetoothUuid, QSharedPointer<QLowEnergyServicePrivate> > ServiceDataMap;
class QLeAdvertiser;

class QLowEnergyControllerPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QLowEnergyController)
public:
    QLowEnergyControllerPrivate();
    ~QLowEnergyControllerPrivate();

    void setError(QLowEnergyController::Error newError);
    bool isValidLocalAdapter();

    void setState(QLowEnergyController::ControllerState newState);

    void connectToDevice();
    void disconnectFromDevice();

    void discoverServices();
    void invalidateServices();

    void discoverServiceDetails(const QBluetoothUuid &service);

    void startAdvertising(const QLowEnergyAdvertisingParameters &params,
                          const QLowEnergyAdvertisingData &advertisingData,
                          const QLowEnergyAdvertisingData &scanResponseData);
    void stopAdvertising();

    // misc helpers
    QSharedPointer<QLowEnergyServicePrivate> serviceForHandle(
            QLowEnergyHandle handle);
    QLowEnergyCharacteristic characteristicForHandle(
            QLowEnergyHandle handle);
    QLowEnergyDescriptor descriptorForHandle(
            QLowEnergyHandle handle);

    quint16 updateValueOfCharacteristic(QLowEnergyHandle charHandle,
                                     const QByteArray &value,
                                     bool appendValue);
    quint16 updateValueOfDescriptor(QLowEnergyHandle charHandle,
                                 QLowEnergyHandle descriptorHandle,
                                 const QByteArray &value,
                                 bool appendValue);

    // read data
    void readCharacteristic(const QSharedPointer<QLowEnergyServicePrivate> service,
                            const QLowEnergyHandle charHandle);
    void readDescriptor(const QSharedPointer<QLowEnergyServicePrivate> service,
                        const QLowEnergyHandle charHandle,
                        const QLowEnergyHandle descriptorHandle);

    // write data
    void writeCharacteristic(const QSharedPointer<QLowEnergyServicePrivate> service,
                             const QLowEnergyHandle charHandle,
                             const QByteArray &newValue, bool writeWithResponse = true);
    void writeDescriptor(const QSharedPointer<QLowEnergyServicePrivate> service,
                         const QLowEnergyHandle charHandle,
                         const QLowEnergyHandle descriptorHandle,
                         const QByteArray &newValue);

    void addToGenericAttributeList(const QLowEnergyServiceData &service,
                                   QLowEnergyHandle startHandle);

    QBluetoothAddress remoteDevice;
    QBluetoothAddress localAdapter;
    QLowEnergyController::Role role;

    QString remoteName;

    QLowEnergyController::ControllerState state;
    QLowEnergyController::Error error;
    QString errorString;

    // list of all found service uuids
    ServiceDataMap serviceList;

    QLowEnergyHandle lastLocalHandle;
    ServiceDataMap localServices;

    struct Attribute {
        Attribute() : handle(0) {}

        QLowEnergyHandle handle;
        QLowEnergyHandle groupEndHandle;
        QLowEnergyCharacteristic::PropertyTypes properties;
        QBluetooth::AttAccessConstraints readConstraints;
        QBluetooth::AttAccessConstraints writeConstraints;
        QBluetoothUuid type;
        QByteArray value;
        int minLength;
        int maxLength;
    };
    QVector<Attribute> localAttributes;

    QLowEnergyController::RemoteAddressType addressType;

private:
#if defined(QT_BLUEZ_BLUETOOTH) && !defined(QT_BLUEZ_NO_BTLE)
    QBluetoothSocket *l2cpSocket;
    struct Request {
        quint8 command;
        QByteArray payload;
        // TODO reference below is ugly but until we know all commands and their
        // requirements this is WIP
        QVariant reference;
        QVariant reference2;
    };
    QQueue<Request> openRequests;

    struct WriteRequest {
        WriteRequest() {}
        WriteRequest(quint16 h, quint16 o, const QByteArray &v)
            : handle(h), valueOffset(o), value(v) {}
        quint16 handle;
        quint16 valueOffset;
        QByteArray value;
    };
    QVector<WriteRequest> openPrepareWriteRequests;

    // Invariant: !scheduledIndications.isEmpty => indicationInFlight == true
    QVector<QLowEnergyHandle> scheduledIndications;
    bool indicationInFlight = false;

    struct TempClientConfigurationData {
        TempClientConfigurationData(QLowEnergyServicePrivate::DescData *dd = nullptr,
                                    QLowEnergyHandle chHndl = 0, QLowEnergyHandle coHndl = 0)
            : descData(dd), charValueHandle(chHndl), configHandle(coHndl) {}

        QLowEnergyServicePrivate::DescData *descData;
        QLowEnergyHandle charValueHandle;
        QLowEnergyHandle configHandle;
    };

    struct ClientConfigurationData {
        ClientConfigurationData(QLowEnergyHandle chHndl = 0, QLowEnergyHandle coHndl = 0,
                                quint16 val = 0)
            : charValueHandle(chHndl), configHandle(coHndl), configValue(val) {}

        QLowEnergyHandle charValueHandle;
        QLowEnergyHandle configHandle;
        quint16 configValue;
        bool charValueWasUpdated = false;
    };
    QHash<quint64, QVector<ClientConfigurationData>> clientConfigData;

    bool requestPending;
    quint16 mtuSize;
    int securityLevelValue;
    bool encryptionChangePending;
    bool receivedMtuExchangeRequest = false;

    HciManager *hciManager;
    QLeAdvertiser *advertiser;
    QSocketNotifier *serverSocketNotifier;

    void handleConnectionRequest();
    void closeServerSocket();

    bool isBonded() const;
    QVector<TempClientConfigurationData> gatherClientConfigData();
    void storeClientConfigurations();
    void restoreClientConfigurations();

    void sendPacket(const QByteArray &packet);
    void sendNextPendingRequest();
    void processReply(const Request &request, const QByteArray &reply);

    void sendReadByGroupRequest(QLowEnergyHandle start, QLowEnergyHandle end,
                                quint16 type);
    void sendReadByTypeRequest(QSharedPointer<QLowEnergyServicePrivate> serviceData,
                               QLowEnergyHandle nextHandle, quint16 attributeType);
    void sendReadValueRequest(QLowEnergyHandle attributeHandle, bool isDescriptor);
    void readServiceValues(const QBluetoothUuid &service,
                           bool readCharacteristics);
    void readServiceValuesByOffset(uint handleData, quint16 offset,
                                   bool isLastValue);

    void discoverServiceDescriptors(const QBluetoothUuid &serviceUuid);
    void discoverNextDescriptor(QSharedPointer<QLowEnergyServicePrivate> serviceData,
                                const QList<QLowEnergyHandle> pendingCharHandles,
                                QLowEnergyHandle startingHandle);
    void processUnsolicitedReply(const QByteArray &msg);
    void exchangeMTU();
    bool setSecurityLevel(int level);
    int securityLevel() const;
    void sendExecuteWriteRequest(const QLowEnergyHandle attrHandle,
                                 const QByteArray &newValue,
                                 bool isCancelation);
    void sendNextPrepareWriteRequest(const QLowEnergyHandle handle,
                                     const QByteArray &newValue, quint16 offset);
    bool increaseEncryptLevelfRequired(quint8 errorCode);

    void resetController();

    void handleAdvertisingError();

    bool checkPacketSize(const QByteArray &packet, int minSize, int maxSize = -1);
    bool checkHandle(const QByteArray &packet, QLowEnergyHandle handle);
    bool checkHandlePair(quint8 request, QLowEnergyHandle startingHandle,
                         QLowEnergyHandle endingHandle);

    void handleExchangeMtuRequest(const QByteArray &packet);
    void handleFindInformationRequest(const QByteArray &packet);
    void handleFindByTypeValueRequest(const QByteArray &packet);
    void handleReadByTypeRequest(const QByteArray &packet);
    void handleReadRequest(const QByteArray &packet);
    void handleReadBlobRequest(const QByteArray &packet);
    void handleReadMultipleRequest(const QByteArray &packet);
    void handleReadByGroupTypeRequest(const QByteArray &packet);
    void handleWriteRequestOrCommand(const QByteArray &packet);
    void handlePrepareWriteRequest(const QByteArray &packet);
    void handleExecuteWriteRequest(const QByteArray &packet);

    void sendErrorResponse(quint8 request, quint16 handle, quint8 code);

    using ElemWriter = std::function<void(const Attribute &, char *&)>;
    void sendListResponse(const QByteArray &packetStart, int elemSize,
                          const QVector<Attribute> &attributes, const ElemWriter &elemWriter);

    void sendNotification(QLowEnergyHandle handle);
    void sendIndication(QLowEnergyHandle handle);
    void sendNotificationOrIndication(quint8 opCode, QLowEnergyHandle handle);
    void sendNextIndication();

    void ensureUniformAttributes(QVector<Attribute> &attributes, const std::function<int(const Attribute &)> &getSize);
    void ensureUniformUuidSizes(QVector<Attribute> &attributes);
    void ensureUniformValueSizes(QVector<Attribute> &attributes);

    using AttributePredicate = std::function<bool(const Attribute &)>;
    QVector<Attribute> getAttributes(QLowEnergyHandle startHandle, QLowEnergyHandle endHandle,
            const AttributePredicate &attributePredicate = [](const Attribute &) { return true; });

    int checkPermissions(const Attribute &attr, QLowEnergyCharacteristic::PropertyType type);
    int checkReadPermissions(const Attribute &attr);
    int checkReadPermissions(QVector<Attribute> &attributes);

    void updateLocalAttributeValue(
            QLowEnergyHandle handle,
            const QByteArray &value,
            QLowEnergyCharacteristic &characteristic,
            QLowEnergyDescriptor &descriptor);

    void writeCharacteristicForPeripheral(
            QLowEnergyServicePrivate::CharData &charData,
            const QByteArray &newValue);
    void writeCharacteristicForCentral(
            QLowEnergyHandle charHandle,
            QLowEnergyHandle valueHandle,
            const QByteArray &newValue,
            bool writeWithResponse);

    void writeDescriptorForPeripheral(
            const QSharedPointer<QLowEnergyServicePrivate> &service,
            const QLowEnergyHandle charHandle,
            const QLowEnergyHandle descriptorHandle,
            const QByteArray &newValue);
    void writeDescriptorForCentral(
            const QLowEnergyHandle charHandle,
            const QLowEnergyHandle descriptorHandle,
            const QByteArray &newValue);

private slots:
    void l2cpConnected();
    void l2cpDisconnected();
    void l2cpErrorChanged(QBluetoothSocket::SocketError);
    void l2cpReadyRead();
    void encryptionChangedEvent(const QBluetoothAddress&, bool);
#elif defined(QT_ANDROID_BLUETOOTH)
    LowEnergyNotificationHub *hub;

private slots:
    void connectionUpdated(QLowEnergyController::ControllerState newState,
                           QLowEnergyController::Error errorCode);
    void servicesDiscovered(QLowEnergyController::Error errorCode,
                            const QString &foundServices);
    void serviceDetailsDiscoveryFinished(const QString& serviceUuid,
                                         int startHandle, int endHandle);
    void characteristicRead(const QBluetoothUuid &serviceUuid, int handle,
                            const QBluetoothUuid &charUuid, int properties,
                            const QByteArray& data);
    void descriptorRead(const QBluetoothUuid &serviceUuid, const QBluetoothUuid &charUuid,
                        int handle, const QBluetoothUuid &descUuid, const QByteArray &data);
    void characteristicWritten(int charHandle, const QByteArray &data,
                               QLowEnergyService::ServiceError errorCode);
    void descriptorWritten(int descHandle, const QByteArray &data,
                           QLowEnergyService::ServiceError errorCode);
    void characteristicChanged(int charHandle, const QByteArray &data);
    void serviceError(int attributeHandle, QLowEnergyService::ServiceError errorCode);
#endif
private:
    QLowEnergyController *q_ptr;

};

Q_DECLARE_TYPEINFO(QLowEnergyControllerPrivate::Attribute, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QT_OSX_BLUETOOTH || QT_IOS_BLUETOOTH

#endif // QLOWENERGYCONTROLLERPRIVATE_P_H
