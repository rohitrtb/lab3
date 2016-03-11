/*
use this for lab3. Commit and push such that no conflicts are generated.
In case of conflicts, please roll back and resolve conficts.
*/

/*Some common header files*/
#include "ns3/internet-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/olsr-module.h"
#include "ns3/aodv-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/traced-value.h"
#include "ns3/queue.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/position-allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <string.h>

using namespace ns3;
using namespace std;

//Setting up global variable for keeping a track of transmitted bytes and received bytes

uint32_t transmitted_bytes = 0;
uint32_t received_bytes = 0;

//A simple method to update the transmitted bytes
void Trace(Ptr<const Packet> packet_value) {
	transmitted_bytes += packet_value->GetSize();
}

int main(int argc, char* argv[]) {

    SeedManager::SetSeed(11223344); //Keep a common seed to compare results
    Ptr<UniformRandomVariable> start_time = CreateObject<UniformRandomVariable> ();
    Ptr<UniformRandomVariable> random_peers = CreateObject<UniformRandomVariable> ();

    uint32_t num_nodes = 20; 
    double intensity = 0.9;
    double power = 1.0; //corresponds to 1mW
    uint32_t area = 1000; //Area of 1000*1000.
    uint32_t area_sq = area*area;
    double node_density = num_nodes/area_sq; //default value is 20/1000000
    //double tx_rate;
    double efficiency;
    double max_tx_rate = 11000000; //11Mbps
    
    //Size of the area in String to easily pass to functions
    string area_str = "1000";
    string route_prot = "AODV";
    string Mode ("DsssRate1Mbps");
    
    CommandLine cmd;
    cmd.AddValue("num_nodes", "Number of nodes in the simulation", num_nodes);
    cmd.AddValue("intensity", "Intensity varying between 0.1 and 0.9", intensity);
    cmd.AddValue("power", "Transmission power in mW", power);
    cmd.AddValue("route_prot", "Routing Protocol: AODV or OLSR", route_prot);
    cmd.Parse (argc, argv);

    Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue(512));
    double rate = (intensity * max_tx_rate)/num_nodes;

    NodeContainer nodes;
    nodes.Create(num_nodes);

    Ipv4ListRoutingHelper list;
    InternetStackHelper internet;

    //Setting up wireless properties for the nodes
    if (route_prot == "AODV") {
	AodvHelper aodv;
	list.Add(aodv, 100);
	internet.SetRoutingHelper (list);
    } else if (route_prot == "OLSR") {
	OlsrHelper olsr;
	list.Add(olsr, 100);
	internet.SetRoutingHelper (list);
    }
    internet.Install(nodes);

    //Setting up channel and transmission properties
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
   
    WifiHelper wifi;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.Set("RxGain",DoubleValue(0));
    double power_db = 10 * log10 (power);
    phy.Set("TxPowerStart", DoubleValue(power_db));
    phy.Set("TxPowerEnd", DoubleValue(power_db));
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
				  "DataMode", StringValue(Mode),
				  "ControlMode", StringValue (Mode));
    mac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer wifi_routers = wifi.Install(phy,mac,nodes);

    //Placing nodes in the area
    MobilityHelper mobility;
    string random_place = "ns3::UniformRandomVariable[Min=0.0|Max="+ area_str + "]";
    mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
				   "X", StringValue (random_place),
				   "Y", StringValue (random_place));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    //Assigning IP addresses to the nodes
    Ipv4AddressHelper address;
    address.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interface;
    interface = address.Assign(wifi_routers);

    //Create pairs and install the on/off applications

    double port = 4003;
    	ApplicationContainer sourceApp[num_nodes];
    ApplicationContainer sinkApp[num_nodes];
    //start_time, random_peers

    start_time->SetAttribute ("Min", DoubleValue(0.0));
    start_time->SetAttribute ("Max", DoubleValue(1.0));
    random_peers->SetAttribute ("Min", DoubleValue(0.0));
    random_peers->SetAttribute ("Max", DoubleValue(num_nodes-1));

    //array for storing start times of each node
    double start_times[num_nodes] ;
    double pairs[num_nodes];
    for (uint32_t i=0; i<num_nodes; i++) {
	start_times[i] = start_time->GetValue();
        OnOffHelper UDPHelper("ns3::UdpSocketFactory",Address());
	UDPHelper.SetConstantRate(DataRate(rate));
        pairs[i] = random_peers->GetValue();
	while (pairs[i] == i) {
		pairs[i] = random_peers->GetValue();
	}
	AddressValue Addr (InetSocketAddress(interface.GetAddress(pairs[i]),port));
	UDPHelper.SetAttribute("Remote",Addr);
 
        sourceApp[i] = UDPHelper.Install(nodes.Get(i));
	sourceApp[i].Start(Seconds(start_times[i]));
	sourceApp[i].Stop(Seconds(10.0));

	PacketSinkHelper UDPSink("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(),port));
	sinkApp[i] = UDPSink.Install(nodes.Get(i));
	sinkApp[i].Start(Seconds(0.0));
	sinkApp[i].Stop(Seconds (10.0));

	Ptr<OnOffApplication> source1 = DynamicCast<OnOffApplication> (sourceApp[i].Get(0));
	source1->TraceConnectWithoutContext ("Tx",MakeCallback(&Trace));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    AnimationInterface animate("animation_lab3.xml");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    for (uint32_t i = 0; i<num_nodes; i++) {
	Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApp[i].Get(0));
	received_bytes += sink1->GetTotalRx();
	cout<<"Sink App "<<i<<": "<<sink1->GetTotalRx()<<endl;
    }

    efficiency = (double)received_bytes/(double)transmitted_bytes;
    
    cout<<"Nodes\t"<<"Area\t"<<"Node Density\t"<<"Intensity\t"<<"Protocol\t"<<"Power mW\t"<<"Efficiency\t"<<endl;
    cout<<num_nodes<<"\t"<<area_sq<<"\t"<<node_density<<"\t"<<intensity<<"\t"<<route_prot<<"\t"<<power<<"\t"<<efficiency<<endl;

    Simulator::Destroy();
    return 0;
}
