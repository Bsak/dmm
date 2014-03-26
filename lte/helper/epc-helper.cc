/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jnin@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 *         Manuel Requena <manuel.requena@cttc.es>
 */

#include <ns3/epc-helper.h>
#include <ns3/log.h>
#include <ns3/inet-socket-address.h>
#include <ns3/mac48-address.h>
#include <ns3/eps-bearer.h>
#include <ns3/ipv4-address.h>
#include <ns3/internet-stack-helper.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/ipv4-static-routing-helper.h>
#include <ns3/ipv4-static-routing.h>
#include <ns3/packet-socket-helper.h>
#include <ns3/packet-socket-address.h>
#include <ns3/epc-enb-application.h>
#include <ns3/epc-sgw-pgw-application.h>

#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-ue-rrc.h>
#include <ns3/epc-x2.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-ue-net-device.h>
#include <ns3/epc-mme.h>
#include <ns3/epc-ue-nas.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcHelper");

NS_OBJECT_ENSURE_REGISTERED (EpcHelper);


EpcHelper::EpcHelper ()
  : m_gtpuUdpPort (2152)  // fixed by the standard
{
  NS_LOG_FUNCTION (this);

  // since we use point-to-point links for all S1-U links,
  // we use a /30 subnet which can hold exactly two addresses
  // (remember that net broadcast and null address are not valid)
  m_s1uIpv4AddressHelper.SetBase ("10.0.0.0", "255.255.255.252");

  m_x2Ipv4AddressHelper.SetBase ("12.0.0.0", "255.255.255.252");

  // we use a /8 net for all UEs
  m_ueAddressHelper.SetBase ("7.0.0.0", "255.0.0.0");

  // create SgwPgwNode
  m_sgwPgw = CreateObject<Node> ();
  InternetStackHelper internet;
  internet.Install (m_sgwPgw);

  // create S1-U socket
  Ptr<Socket> sgwPgwS1uSocket = Socket::CreateSocket (m_sgwPgw, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = sgwPgwS1uSocket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_gtpuUdpPort));
  NS_ASSERT (retval == 0);

  // create TUN device implementing tunneling of user data over GTP-U/UDP/IP
  m_tunDevice = CreateObject<VirtualNetDevice> ();
  // allow jumbo packets
  m_tunDevice->SetAttribute ("Mtu", UintegerValue (30000));

  // yes we need this
  m_tunDevice->SetAddress (Mac48Address::Allocate ());

  m_sgwPgw->AddDevice (m_tunDevice);
  NetDeviceContainer tunDeviceContainer;
  tunDeviceContainer.Add (m_tunDevice);

  // the TUN device is on the same subnet as the UEs, so when a packet
  // addressed to an UE arrives at the intenet to the WAN interface of
  // the PGW it will be forwarded to the TUN device.
  Ipv4InterfaceContainer tunDeviceIpv4IfContainer = m_ueAddressHelper.Assign (tunDeviceContainer);

  // create EpcSgwPgwApplication
  m_sgwPgwApp = CreateObject<EpcSgwPgwApplication> (m_tunDevice, sgwPgwS1uSocket);
  m_sgwPgw->AddApplication (m_sgwPgwApp);

  // connect SgwPgwApplication and virtual net device for tunneling
  m_tunDevice->SetSendCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromTunDevice, m_sgwPgwApp));

  // Create MME and connect with SGW via S11 interface
  m_mme = CreateObject<EpcMme> ();
  m_mme->SetS11SapSgw (m_sgwPgwApp->GetS11SapSgw ());
  m_sgwPgwApp->SetS11SapMme (m_mme->GetS11SapMme ());
}

EpcHelper::EpcHelper (Ipv4AddressHelper pgwPool, Ipv4AddressHelper s1Pool,
                      Ipv4AddressHelper x2Pool, Ptr<Node> pgw)
  : m_gtpuUdpPort (2152)  // fixed by the standard
{
  NS_LOG_FUNCTION (this);

  m_s1uIpv4AddressHelper = s1Pool;

  m_x2Ipv4AddressHelper = x2Pool;

  m_ueAddressHelper = pgwPool;

  // create SgwPgwNode
  m_sgwPgw = pgw;

  // create S1-U socket
  Ptr<Socket> sgwPgwS1uSocket = Socket::CreateSocket (m_sgwPgw, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = sgwPgwS1uSocket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_gtpuUdpPort));
  NS_ASSERT (retval == 0);

  // create TUN device implementing tunneling of user data over GTP-U/UDP/IP
  m_tunDevice = CreateObject<VirtualNetDevice> ();
  // allow jumbo packets
  m_tunDevice->SetAttribute ("Mtu", UintegerValue (30000));

  // yes we need this
  m_tunDevice->SetAddress (Mac48Address::Allocate ());

  m_sgwPgw->AddDevice (m_tunDevice);
  NetDeviceContainer tunDeviceContainer;
  tunDeviceContainer.Add (m_tunDevice);

  // the TUN device is on the same subnet as the UEs, so when a packet
  // addressed to an UE arrives at the intenet to the WAN interface of
  // the PGW it will be forwarded to the TUN device.
  Ipv4InterfaceContainer tunDeviceIpv4IfContainer = m_ueAddressHelper.Assign (tunDeviceContainer);

  // create EpcSgwPgwApplication
  m_sgwPgwApp = CreateObject<EpcSgwPgwApplication> (m_tunDevice, sgwPgwS1uSocket);
  m_sgwPgw->AddApplication (m_sgwPgwApp);

  // connect SgwPgwApplication and virtual net device for tunneling
  m_tunDevice->SetSendCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromTunDevice, m_sgwPgwApp));

  // Create MME and connect with SGW via S11 interface
  m_mme = CreateObject<EpcMme> ();
  m_mme->SetS11SapSgw (m_sgwPgwApp->GetS11SapSgw ());
  m_sgwPgwApp->SetS11SapMme (m_mme->GetS11SapMme ());
}

Ipv4AddressHelper
EpcHelper::GetHelper ()
{
    return m_ueAddressHelper;
}

void
EpcHelper::ReSetHelper (Ipv4AddressHelper h)
{
    m_ueAddressHelper = h;
}

EpcHelper::EpcHelper (Ptr<Node> pgw)
  : m_gtpuUdpPort (2152), // fixed by the standard
    m_current(0),
    m_count(0)
{
  NS_LOG_FUNCTION (this);

  Ipv4Address prefix = pgw->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal();
  uint8_t buf[4];
  prefix.Serialize(buf);
  buf[3] = 0;
  prefix = prefix.Deserialize(buf);
  m_ueAddressHelper.SetBase (prefix, "255.255.255.0");
  m_ueAddressHelper.NewNetwork();

  // create SgwPgwNode
  m_sgwPgw = pgw;
  m_mac = Mac48Address::ConvertFrom(pgw->GetDevice(1)->GetAddress());

  // create S1-U socket
  Ptr<Socket> sgwPgwS1uSocket = Socket::CreateSocket (m_sgwPgw, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = sgwPgwS1uSocket->Bind (InetSocketAddress (Ipv4Address::GetAny (), m_gtpuUdpPort));
  NS_ASSERT (retval == 0);

  // create TUN device implementing tunneling of user data over GTP-U/UDP/IP
  m_tunDevice = CreateObject<VirtualNetDevice> ();
  // allow jumbo packets
  m_tunDevice->SetAttribute ("Mtu", UintegerValue (30000));

  // yes we need this
  m_tunDevice->SetAddress (Mac48Address::Allocate ());

  m_sgwPgw->AddDevice (m_tunDevice);
  NetDeviceContainer tunDeviceContainer;
  tunDeviceContainer.Add (m_tunDevice);

  // the TUN device is on the same subnet as the UEs, so when a packet
  // addressed to an UE arrives at the intenet to the WAN interface of
  // the PGW it will be forwarded to the TUN device.
  Ipv4InterfaceContainer tunDeviceIpv4IfContainer = m_ueAddressHelper.Assign (tunDeviceContainer);

  // create EpcSgwPgwApplication
  m_sgwPgwApp = CreateObject<EpcSgwPgwApplication> (m_tunDevice, sgwPgwS1uSocket);
  m_sgwPgw->AddApplication (m_sgwPgwApp);

  // connect SgwPgwApplication and virtual net device for tunneling
  m_tunDevice->SetSendCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromTunDevice, m_sgwPgwApp));

    // Create MME and connect with SGW via S11 interface
  m_mme = CreateObject<EpcMme> ();
  m_mme->SetS11SapSgw (m_sgwPgwApp->GetS11SapSgw ());
  m_sgwPgwApp->SetS11SapMme (m_mme->GetS11SapMme ());
}

EpcHelper::~EpcHelper ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
EpcHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EpcHelper")
    .SetParent<Object> ()
    .AddConstructor<EpcHelper> ()
    .AddAttribute ("S1uLinkDataRate",
                   "The data rate to be used for the next S1-U link to be created",
                   DataRateValue (DataRate ("10Gb/s")),
                   MakeDataRateAccessor (&EpcHelper::m_s1uLinkDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("S1uLinkDelay",
                   "The delay to be used for the next S1-U link to be created",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&EpcHelper::m_s1uLinkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("S1uLinkMtu",
                   "The MTU of the next S1-U link to be created. Note that, because of the additional GTP/UDP/IP tunneling overhead, you need a MTU larger than the end-to-end MTU that you want to support.",
                   UintegerValue (2000),
                   MakeUintegerAccessor (&EpcHelper::m_s1uLinkMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("X2LinkDataRate",
                   "The data rate to be used for the next X2 link to be created",
                   DataRateValue (DataRate ("10Gb/s")),
                   MakeDataRateAccessor (&EpcHelper::m_x2LinkDataRate),
                   MakeDataRateChecker ())
    .AddAttribute ("X2LinkDelay",
                   "The delay to be used for the next X2 link to be created",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&EpcHelper::m_x2LinkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("X2LinkMtu",
                   "The MTU of the next X2 link to be created. Note that, because of some big X2 messages, you need a big MTU.",
                   UintegerValue (3000),
                   MakeUintegerAccessor (&EpcHelper::m_x2LinkMtu),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

void
EpcHelper::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_tunDevice->SetSendCallback (MakeNullCallback<bool, Ptr<Packet>, const Address&, const Address&, uint16_t> ());
  m_tunDevice = 0;
  m_sgwPgwApp = 0;
  m_sgwPgw->Dispose ();
}

Ipv4AddressHelper
EpcHelper::AddTunDevice(Ipv4AddressHelper ipv4ah)
{
  Ptr<VirtualNetDevice> tunDevice = CreateObject<VirtualNetDevice>();

  // allow jumbo packets
  tunDevice->SetAttribute ("Mtu", UintegerValue (30000));

  // yes we need this
  tunDevice->SetAddress (Mac48Address::Allocate ());

  m_sgwPgw->AddDevice (tunDevice);
  NetDeviceContainer tunDeviceContainer;
  tunDeviceContainer.Add (tunDevice);

  // the TUN device is on the same subnet as the UEs, so when a packet
  // addressed to an UE arrives at the intenet to the WAN interface of
  // the     PGW it will be forwarded to the TUN device.
  Ipv4InterfaceContainer tunDeviceIpv4IfContainer = ipv4ah.Assign (tunDeviceContainer);

  // connect SgwPgwApplication and virtual net device for tunneling
  tunDevice->SetSendCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromTunDevice, m_sgwPgwApp));

  return ipv4ah;
}


void
EpcHelper::AddEnb (Ptr<Node> enb, Ptr<NetDevice> lteEnbNetDevice, uint16_t cellId)
{
  NS_LOG_FUNCTION (this << enb << lteEnbNetDevice << cellId);

  NS_ASSERT (enb == lteEnbNetDevice->GetNode ());

  // add an IPv4 stack to the previously created eNB
  /*InternetStackHelper internet;
  internet.Install (enb);*/
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after node creation: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());

  // create a point to point link between the new eNB and the SGW with
  // the corresponding new NetDevices on each side
  /*NodeContainer enbSgwNodes;
  enbSgwNodes.Add (m_sgwPgw);
  enbSgwNodes.Add (enb);
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (m_s1uLinkDataRate));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (m_s1uLinkMtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (m_s1uLinkDelay));
  NetDeviceContainer enbSgwDevices = p2ph.Install (enb, m_sgwPgw);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after installing p2p dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());
  Ptr<NetDevice> enbDev = enbSgwDevices.Get (0);
  Ptr<NetDevice> sgwDev = enbSgwDevices.Get (1);
  m_s1uIpv4AddressHelper.NewNetwork ();
  Ipv4InterfaceContainer enbSgwIpIfaces = m_s1uIpv4AddressHelper.Assign (enbSgwDevices);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB after assigning Ipv4 addr to S1 dev: " << enb->GetObject<Ipv4> ()->GetNInterfaces ());
*/
  Ipv4Address enbAddress = enb->GetObject<Ipv4> ()->GetAddress(1,0).GetLocal();
  Ipv4Address sgwAddress = GetUeDefaultGatewayAddress();

  // create S1-U socket for the ENB
  Ptr<Socket> enbS1uSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::UdpSocketFactory"));
  int retval = enbS1uSocket->Bind (InetSocketAddress (enbAddress, m_gtpuUdpPort));
  NS_ASSERT (retval == 0);


  // give PacketSocket powers to the eNB
  //PacketSocketHelper packetSocket;
  //packetSocket.Install (enb);

  // create LTE socket for the ENB
  Ptr<Socket> enbLteSocket = Socket::CreateSocket (enb, TypeId::LookupByName ("ns3::PacketSocketFactory"));
  PacketSocketAddress enbLteSocketBindAddress;
  enbLteSocketBindAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketBindAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Bind (enbLteSocketBindAddress);
  NS_ASSERT (retval == 0);
  PacketSocketAddress enbLteSocketConnectAddress;
  enbLteSocketConnectAddress.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  enbLteSocketConnectAddress.SetSingleDevice (lteEnbNetDevice->GetIfIndex ());
  enbLteSocketConnectAddress.SetProtocol (Ipv4L3Protocol::PROT_NUMBER);
  retval = enbLteSocket->Connect (enbLteSocketConnectAddress);
  NS_ASSERT (retval == 0);


  NS_LOG_INFO ("create EpcEnbApplication");
  Ptr<EpcEnbApplication> enbApp = CreateObject<EpcEnbApplication> (enbLteSocket, enbS1uSocket, enbAddress, sgwAddress, cellId);
  enb->AddApplication (enbApp);
  NS_ASSERT (enb->GetNApplications () == 1);
  NS_ASSERT_MSG (enb->GetApplication (0)->GetObject<EpcEnbApplication> () != 0, "cannot retrieve EpcEnbApplication");
  NS_LOG_LOGIC ("enb: " << enb << ", enb->GetApplication (0): " << enb->GetApplication (0));

  lteEnbNetDevice->GetObject<LteEnbNetDevice>()->GetRrc()->SetEnbApp(enbApp);

  NS_LOG_INFO ("Create EpcX2 entity");
  Ptr<EpcX2> x2 = CreateObject<EpcX2> ();
  enb->AggregateObject (x2);

  NS_LOG_INFO ("connect S1-AP interface");
  m_mme->AddEnb (cellId, enbAddress, enbApp->GetS1apSapEnb ());
  m_sgwPgwApp->AddEnb (cellId, enbAddress, sgwAddress);
  enbApp->SetS1apSapMme (m_mme->GetS1apSapMme ());
}

void
EpcHelper::AddX2Interface (Ptr<Node> enb1, Ptr<Node> enb2)
{
  NS_LOG_FUNCTION (this << enb1 << enb2);

  // Create a point to point link between the two eNBs with
  // the corresponding new NetDevices on each side
  /*NodeContainer enbNodes;
  enbNodes.Add (enb1);
  enbNodes.Add (enb2);
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (m_x2LinkDataRate));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (m_x2LinkMtu));
  p2ph.SetChannelAttribute ("Delay", TimeValue (m_x2LinkDelay));
  NetDeviceContainer enbDevices = p2ph.Install (enb1, enb2);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #1 after installing p2p dev: " << enb1->GetObject<Ipv4> ()->GetNInterfaces ());
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #2 after installing p2p dev: " << enb2->GetObject<Ipv4> ()->GetNInterfaces ());
  Ptr<NetDevice> enb1Dev = enbDevices.Get (0);
  Ptr<NetDevice> enb2Dev = enbDevices.Get (1);

  m_x2Ipv4AddressHelper.NewNetwork ();
  Ipv4InterfaceContainer enbIpIfaces = m_x2Ipv4AddressHelper.Assign (enbDevices);
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #1 after assigning Ipv4 addr to X2 dev: " << enb1->GetObject<Ipv4> ()->GetNInterfaces ());
  NS_LOG_LOGIC ("number of Ipv4 ifaces of the eNB #2 after assigning Ipv4 addr to X2 dev: " << enb2->GetObject<Ipv4> ()->GetNInterfaces ());
*/
  Ipv4Address enb1X2Address = enb1->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
  Ipv4Address enb2X2Address = enb2->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

  // Add X2 interface to both eNBs' X2 entities
  Ptr<EpcX2> enb1X2 = enb1->GetObject<EpcX2> ();
  Ptr<LteEnbNetDevice> enb1LteDev = enb1->GetDevice (3)->GetObject<LteEnbNetDevice> ();
  uint16_t enb1CellId = enb1LteDev->GetCellId ();
  NS_LOG_LOGIC ("LteEnbNetDevice #1 = " << enb1LteDev << " - CellId = " << enb1CellId);

  Ptr<EpcX2> enb2X2 = enb2->GetObject<EpcX2> ();
  Ptr<LteEnbNetDevice> enb2LteDev = enb2->GetDevice (3)->GetObject<LteEnbNetDevice> ();
  uint16_t enb2CellId = enb2LteDev->GetCellId ();
  NS_LOG_LOGIC ("LteEnbNetDevice #2 = " << enb2LteDev << " - CellId = " << enb2CellId);

  enb1X2->AddX2Interface (enb1CellId, enb1X2Address, enb2CellId, enb2X2Address);
  enb2X2->AddX2Interface (enb2CellId, enb2X2Address, enb1CellId, enb1X2Address);
}


void
EpcHelper::AddUe (Ptr<NetDevice> ueDevice, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << imsi << ueDevice );

  m_mme->AddUe (imsi);
  m_sgwPgwApp->AddUe (imsi);
}

void
EpcHelper::ActivateEpsBearer (Ptr<NetDevice> ueDevice, uint64_t imsi, Ptr<EpcTft> tft, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this << ueDevice << imsi);

  // we now retrieve the IPv4 address of the UE and notify it to the SGW;
  // we couldn't do it before since address assignment is triggered by
  // the user simulation program, rather than done by the EPC
  Ptr<Node> ueNode = ueDevice->GetNode ();
  Ptr<Ipv4> ueIpv4 = ueNode->GetObject<Ipv4> ();
  NS_ASSERT_MSG (ueIpv4 != 0, "UEs need to have IPv4 installed before EPS bearers can be activated");
  int32_t interface =  ueIpv4->GetInterfaceForDevice (ueDevice);
  NS_ASSERT (interface >= 0);
  //NS_ASSERT (ueIpv4->GetNAddresses (interface) == 1);
  Ipv4Address ueAddr = ueIpv4->GetAddress (interface, 0).GetLocal ();
  NS_LOG_LOGIC (" UE IP address: " << ueAddr);
  m_sgwPgwApp->SetUeAddress (imsi, ueAddr);

  m_mme->AddBearer (imsi, tft, bearer);
  Ptr<LteUeNetDevice> ueLteDevice = ueDevice->GetObject<LteUeNetDevice> ();
  if (ueLteDevice)
    {
      ueLteDevice->GetNas ()->ActivateEpsBearer (bearer, tft);
    }
}

void
EpcHelper::ModifyBearer(uint64_t imsi, Ipv4Address ueAddr, Ptr<EpcTft> tft, EpsBearer bearer)
{
    m_sgwPgwApp->SetUeAddress (imsi, ueAddr);
    m_mme->AddBearer(imsi, tft, bearer);
    uint8_t bid = m_mme->GetBearerCounter(imsi);
    m_sgwPgwApp->AddBearerAfterHO(tft, bid, imsi, GetLastRnti());
}

Ptr<Node>
EpcHelper::GetPgwNode ()
{
  return m_sgwPgw;
}

Ipv4InterfaceContainer
EpcHelper::AssignUeIpv4Address (NetDeviceContainer ueDevices)
{
  return m_ueAddressHelper.Assign (ueDevices);
}

Ipv4Address
EpcHelper::GetUeDefaultGatewayAddress ()
{
  // return the address of the tun device
  return m_sgwPgw->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
}

Ipv4Address
EpcHelper::Attach (Ptr<NetDevice> ueDevice)
{
  NS_LOG_FUNCTION (this);
  Ptr<LteUeNetDevice> ueLteDevice = ueDevice->GetObject<LteUeNetDevice> ();
  NetDeviceContainer ndc;
  ndc.Add (ueLteDevice);

  // Assign IP address to UE
  AssignUeIpv4Address (ndc);

  Ptr<Node> ueNode = ueDevice->GetNode ();

  uint16_t num = ueNode->GetObject<Ipv4>()->GetNAddresses(1);

  // Set the default gateway for the UE
  Ptr<Ipv4StaticRouting> ueStaticRouting = m_ipv4srh.GetStaticRouting (ueNode->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (GetUeDefaultGatewayAddress (), ueNode->GetObject<Ipv4> ()->GetInterfaceForDevice (ueDevice));

  return ueNode->GetObject<Ipv4>()->GetAddress(1,num-1).GetLocal();
}

void
EpcHelper::SetContainer(const NodeContainer &c)
{
    m_ues.Add(c);
}

uint16_t
EpcHelper::GetLastRnti()
{
    if(m_current != 0){
        return ++m_current;
    }
    uint16_t maxRnti = 0;
    for(uint16_t i=0; i<m_ues.GetN(); i++)
    {
        Ptr<NetDevice> dev = m_ues.Get(i)->GetDevice(0);
        uint16_t rnti = dev->GetObject<LteUeNetDevice> ()->GetRrc ()->GetRnti ();
        if(rnti>maxRnti)
            maxRnti=rnti;
    }
    m_current = ++maxRnti;
    return m_current;
}

/************************ FLOW TABLE MANAGEMENT **************************/
void
EpcHelper::InitOpenFlow(NetDeviceContainer ofDevs, Ptr<ns3::ofi::LearningController> controller) {
    m_ofDevs.Add(ofDevs);
    m_controller = controller;
}

/*void
EpcHelper::InitFlowTable(Ipv4Address ipAddr, Mac48Address addr){
    NS_LOG_FUNCTION (this << ipAddr << addr);
    if(ipAddr.IsBroadcast() && addr.IsBroadcast())
        return;
    UpdateFlowTable(ipAddr, addr);
}*/

void
EpcHelper::UpdateFlowTable(Ipv4Address ipAddr, Mac48Address addr){
  NS_LOG_FUNCTION (this << ipAddr << addr);
  uint8_t buf[4];
  ipAddr.Serialize(buf);
  if(buf[0]==90){ //remoteHost
    if(buf[1]==1){
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 4);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 5);
    }
    else if(buf[1]==2){
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 5);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 5);
    }
    else if(buf[1]==3){
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 5);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 5);
    }
  }
  else if(buf[0]==10){
    if(buf[1]==0){ //sourcePgw pool
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 8);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 5);
    }
    else if(buf[1]==10){ //targetPgw pool
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 6);
    }
    /*
    else if(buf[1]==4){ //sourceEnb
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 4);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[1]==5){ //targetEnb
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
  }
  else if(buf[0]==70){ //controller
    if(controllerPos==0){ //middle of backbone
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(controllerPos==1){ //sgiAR1
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 7);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(controllerPos==2){ //sgiAR2
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 4);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
  }
  else if(buf[0]==192){
    if(buf[3]==1){ //234
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==2){ //235
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[3]==4){ //236
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[3]==6){ //237
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[3]==12){ //310
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[3]==14){ //238
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);

    }
    else if(buf[3]==18){ //312
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==21){ //79
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 4);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==23){ //120
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==24){ //231
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==28){ //258
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==30){ //134
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 3);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(12)), addr, 0);
    }
    else if(buf[3]==32){ //218
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(0)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(1)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(2)), addr, 2);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(3)), addr, 4);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(4)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(5)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(6)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(7)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(8)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(9)), addr, 1);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(10)), addr, 0);
        m_controller->InitFlows(DynamicCast<OpenFlowSwitchNetDevice>(m_ofDevs.Get(11)), addr, 0);
    }*/
  }
}

} // namespace ns3
