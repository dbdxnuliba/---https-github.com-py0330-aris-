﻿
#include <aris_control_motion.h>
#include <iostream>

#ifdef UNIX
#include "ecrt.h"
#endif

int main()
{
	Aris::Core::XmlDocument doc;
#ifdef WIN32
	doc.LoadFile("C:\\Robots\\resource\\Robot_Type_I\\Robot_III.xml");
#endif
#ifdef UNIX
	doc.LoadFile("/usr/Robots/resource/Robot_Type_I/Robot_III.xml");
#endif

#ifdef UNIX


	auto ele = doc.RootElement()->FirstChildElement("Server")
		->FirstChildElement("Control")->FirstChildElement("EtherCat");

	auto pMas = Aris::Control::EthercatMaster::createInstance<Aris::Control::EthercatController>();
	std::cout<<"1"<<std::endl;	
	pMas->loadXml(std::ref(*ele));
	std::cout<<"2"<<std::endl;
	pMas->start();
	std::cout<<"3"<<std::endl;
	
	while (true)
	{
		Aris::Core::Msg msg;
		pMas->msgPipe().recvInNrt(msg);
		std::cout << "NRT msg length:" << msg.size()<<" pos:" << *reinterpret_cast<std::int32_t*>(msg.data())<<std::endl;
		//msg.SetLength(10);
		msg.copy("congratulations\n");
		//pMas->msgPipe().sendToRT(msg);
	}
	
	
#endif
	
	char a;
	std::cin>>a;

	return 0;
}
