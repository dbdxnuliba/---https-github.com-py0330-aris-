#ifdef WIN32
#include <ecrt_windows_py.h>//just for IDE vs2015, it does not really work
#endif
#ifdef UNIX
#include <ecrt.h>
#include <rtdk.h>
#include <native/task.h>
#include <native/timer.h>
#include <sys/mman.h>
// following for pipe
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <rtdm/rtdm.h>
#include <rtdm/rtipc.h>
#include <mutex>
#endif

#include <mutex>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <atomic>
#include <memory>
#include <typeinfo>

#include "aris_control_ethercat.h"

namespace Aris
{
	namespace Control
	{
		struct EthercatSlave::Imp
		{
		public:
			auto read()->void { ecrt_domain_process(domain); };
			auto write()->void { ecrt_domain_queue(domain); };

			class DataObject1
			{
			public:
				virtual ~DataObject1() = default;

				// common data
				Imp *imp_;
				std::string type_name_;
				std::uint16_t index_;
				std::uint8_t subindex_;
				std::uint8_t size_;
				std::uint32_t offset_;

				// for sdo
				union
				{
					char sdo_data_[8];
					std::uint32_t sdo_data_uint32_;
					std::uint16_t sdo_data_uint16_;
					std::uint8_t sdo_data_uint8_;
					std::int32_t sdo_data_int32_;
					std::int16_t sdo_data_int16_;
					std::int8_t sdo_data_int8_;
				};

				template<typename DataType>auto readPdo(DataType&data)->void
				{
					if (typeid(DataType) != *typeInfoMap().at(type_name_).type_info_) throw std::runtime_error("invalid pdo Tx type");
					data = *reinterpret_cast<const DataType*>(imp_->domain_pd + offset_);
				};
				template<typename DataType>auto writePdo(DataType data)->void
				{
					if (typeid(DataType) != *typeInfoMap().at(type_name_).type_info_) throw std::runtime_error("invalid pdo Rx type");
					*reinterpret_cast<DataType*>(imp_->domain_pd + offset_) = data;
				};
				template<typename DataType>auto readSdo(DataType&data)->void
				{
					if(typeid(DataType) != *typeInfoMap().at(type_name_).type_info_)throw std::runtime_error("invalid sdo type");
					data = (*reinterpret_cast<DataType *>(sdo_data_));
				};
				template<typename DataType>auto writeSdo(DataType data)->void
				{
					if (typeid(DataType) != *typeInfoMap().at(type_name_).type_info_)throw std::runtime_error("invalid sdo type");
					(*reinterpret_cast<DataType *>(sdo_data_)) = data;
				};

				class TypeInfo
				{
				public:
					const std::type_info *type_info_;
					std::function<void(const std::string &, void *)> queryFunc;

					std::function<DataObject1*(const Aris::Core::XmlElement &xml_ele, Imp *imp)> newPdo;
					std::function<DataObject1*(const Aris::Core::XmlElement &xml_ele, Imp *imp)> newSdo;
					


					template<typename DataType> static auto createTypeInfo()->TypeInfo
					{
						TypeInfo info;

						info.type_info_ = &typeid(DataType);

						if (typeid(DataType) == typeid(std::int8_t) || typeid(DataType) == typeid(std::int8_t))
						{
							info.queryFunc = [](const std::string &str, void *value) 
							{
								std::int16_t loc_value;
								std::stringstream(str) >> loc_value;
								(*reinterpret_cast<DataType*>(value)) = DataType(loc_value);
							};
						}
						else
						{
							info.queryFunc = [](const std::string &str, void *value) {std::stringstream(str) >> (*reinterpret_cast<DataType*>(value)); };
						}

						

						info.newPdo = [](const Aris::Core::XmlElement &xml_ele, Imp *imp)
						{
							auto ret = new DataObject1;
							ret->type_name_ = xml_ele.Attribute("type");
							ret->index_ = std::stoi(xml_ele.Attribute("index"), nullptr, 0);
							ret->subindex_ = std::stoi(xml_ele.Attribute("subindex"), nullptr, 0);
							ret->imp_ = imp;
							ret->size_ = sizeof(DataType) * 8;
							return ret; 
						};

						info.newSdo = [](const Aris::Core::XmlElement &xml_ele, Imp *imp) -> DataObject1*
						{
							auto ret = new DataObject1;
							ret->type_name_ = xml_ele.Attribute("type");
							ret->index_ = std::stoi(xml_ele.Attribute("index"), nullptr, 0);
							ret->subindex_ = std::stoi(xml_ele.Attribute("subindex"), nullptr, 0);
							ret->imp_ = imp;
							ret->size_ = sizeof(DataType) * 8;
							DataObject1::typeInfoMap().at(xml_ele.Attribute("type")).queryFunc(xml_ele.Attribute("value"), ret->sdo_data_);
							return ret;
						};

						return info;
					}
				};

				static auto typeInfoMap()->const std::map<std::string, TypeInfo> &
				{
					const static std::map<std::string, TypeInfo> info_map
					{
						std::make_pair(std::string("int8"), TypeInfo::createTypeInfo<std::int8_t>()),
						std::make_pair(std::string("uint8"), TypeInfo::createTypeInfo<std::uint8_t>()),
						std::make_pair(std::string("int16"), TypeInfo::createTypeInfo<std::int16_t>()),
						std::make_pair(std::string("uint16"), TypeInfo::createTypeInfo<std::uint16_t>()),
						std::make_pair(std::string("int32"), TypeInfo::createTypeInfo<std::int32_t>()),
						std::make_pair(std::string("uint32"), TypeInfo::createTypeInfo<std::uint32_t>()),
					};

					return std::ref(info_map);
				}
				
			};
			class PdoGroup1
			{
			public:
				bool is_tx;
				uint16_t index;
				std::vector<std::unique_ptr<DataObject1> > pdo_vec;
				std::vector<ec_pdo_entry_info_t> ec_pdo_entry_info_vec;
			};



			/*data object, can be PDO or SDO*/
			std::vector<PdoGroup1> pdo_group_vec;
			std::vector<std::unique_ptr<DataObject1> > sdo_vec;

			std::uint32_t product_code, vender_id;
			std::uint16_t position, alias;
			std::unique_ptr<int> distributed_clock;

			std::vector<ec_pdo_entry_reg_t> ec_pdo_entry_reg_vec;
			std::vector<ec_pdo_info_t> ec_pdo_info_vec_tx, ec_pdo_info_vec_rx;
			ec_sync_info_t ec_sync_info[5];
			ec_slave_config_t* ec_slave_config;
			
			ec_domain_t* domain;
			std::uint8_t* domain_pd;

			friend class EthercatMaster::Imp;
			friend class EthercatSlave;
			friend class EthercatMaster;
		};
		class EthercatMaster::Imp
		{
		public:
			static auto rt_task_func(void *)->void
			{
#ifdef UNIX
				auto &mst = EthercatMaster::instance();

				rt_task_set_periodic(NULL, TM_NOW, mst.imp->sample_period_ns_);

				while (!mst.imp->is_stopping_)
				{
					rt_task_wait_period(NULL);

					mst.imp->read();//motors and sensors get data

					//tg begin
					mst.controlStrategy();
					//tg end

					mst.imp->sync(rt_timer_read());
					mst.imp->write();//motor data write and state machine/mode transition
				}
#endif
			};
			auto read()->void { ecrt_master_receive(ec_master);	for (auto &sla : slave_vec)sla->imp->read(); };
			auto write()->void { for (auto &sla : slave_vec)sla->imp->write(); ecrt_master_send(ec_master); };
			auto sync(uint64_t ns)->void
			{
				ecrt_master_application_time(ec_master, ns);
				ecrt_master_sync_reference_clock(ec_master);
				ecrt_master_sync_slave_clocks(ec_master);
			};

			std::vector<std::unique_ptr<EthercatSlave> > slave_vec;
			ec_master_t* ec_master;

			const int sample_period_ns_ = 1000000;

			std::atomic_bool is_running_{ false }, is_stopping_{ false };

#ifdef UNIX
			RT_TASK rt_task;
#endif
			friend class EthercatSlave;
			friend class EthercatMaster;
		};

		EthercatSlave::EthercatSlave(const Aris::Core::XmlElement &xml_ele):imp(new Imp)
		{
			//load product id...
			imp->product_code = std::stoi(xml_ele.Attribute("product_code"), nullptr, 0);
			imp->vender_id = std::stoi(xml_ele.Attribute("vender_id"), nullptr, 0);
			imp->alias = std::stoi(xml_ele.Attribute("alias"), nullptr, 0);
			imp->distributed_clock.reset(new std::int32_t);

			if (xml_ele.Attribute("distributed_clock"))
			{
				*imp->distributed_clock.get() = std::stoi(xml_ele.Attribute("distributed_clock"), nullptr, 0);
			}
			else
			{
				imp->distributed_clock.reset();
			}


			//load PDO
			auto pdo_xml_ele = xml_ele.FirstChildElement("PDO");
			for (auto p_g = pdo_xml_ele->FirstChildElement(); p_g; p_g = p_g->NextSiblingElement())
			{
				Imp::PdoGroup1 pdo_group;
				pdo_group.index = std::stoi(p_g->Attribute("index"), nullptr, 0);
				pdo_group.is_tx = p_g->Attribute("is_tx", "true") ? true : false;
				for (auto p = p_g->FirstChildElement(); p; p = p->NextSiblingElement())
				{
					pdo_group.pdo_vec.push_back(std::unique_ptr<Imp::DataObject1>(
						Imp::DataObject1::typeInfoMap().at(p->Attribute("type")).newPdo(*p, this->imp.get()))
						);
				}
				imp->pdo_group_vec.push_back(std::move(pdo_group));
			}

			//load SDO
			auto sdo_xml_ele = xml_ele.FirstChildElement("SDO");
			for (auto s = sdo_xml_ele->FirstChildElement(); s; s = s->NextSiblingElement())
			{
				imp->sdo_vec.push_back(std::unique_ptr<Imp::DataObject1>(Imp::DataObject1::typeInfoMap().at(s->Attribute("type")).newSdo(*s, this->imp.get())));
			}

			//create ecrt structs
			for (auto &p_g : imp->pdo_group_vec)
			{
				for (auto &p : p_g.pdo_vec)
				{
					imp->ec_pdo_entry_reg_vec.push_back(ec_pdo_entry_reg_t{ imp->alias, imp->position, imp->vender_id, imp->product_code, p->index_, p->subindex_, &p->offset_ });
					p_g.ec_pdo_entry_info_vec.push_back(ec_pdo_entry_info_t{ p->index_, p->subindex_, p->size_ });
				}

				if (p_g.is_tx)
				{
					imp->ec_pdo_info_vec_tx.push_back(ec_pdo_info_t{ p_g.index, static_cast<std::uint8_t>(p_g.pdo_vec.size()), p_g.ec_pdo_entry_info_vec.data() });
				}
				else
				{
					imp->ec_pdo_info_vec_rx.push_back(ec_pdo_info_t{ p_g.index, static_cast<std::uint8_t>(p_g.pdo_vec.size()), p_g.ec_pdo_entry_info_vec.data() });
				}
			}
			imp->ec_pdo_entry_reg_vec.push_back(ec_pdo_entry_reg_t{});

			imp->ec_sync_info[0] = ec_sync_info_t{ 0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE };
			imp->ec_sync_info[1] = ec_sync_info_t{ 1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE };
			imp->ec_sync_info[2] = ec_sync_info_t{ 2, EC_DIR_OUTPUT, static_cast<unsigned int>(imp->ec_pdo_info_vec_rx.size()), imp->ec_pdo_info_vec_rx.data(), EC_WD_ENABLE };
			imp->ec_sync_info[3] = ec_sync_info_t{ 3, EC_DIR_INPUT, static_cast<unsigned int>(imp->ec_pdo_info_vec_tx.size()),imp->ec_pdo_info_vec_tx.data(), EC_WD_ENABLE };
			imp->ec_sync_info[4] = ec_sync_info_t{ 0xff };
		}
		EthercatSlave::~EthercatSlave() {};
		auto EthercatSlave::init()->void
		{
			auto mst = EthercatMaster::instance().imp->ec_master;

#ifdef UNIX
			for (auto &reg : imp->ec_pdo_entry_reg_vec)	reg.position = imp->position;

			// Create domain
			if (!(imp->domain = ecrt_master_create_domain(mst)))throw std::runtime_error("failed to create domain");

			// Get the slave configuration 
			if (!(imp->ec_slave_config = ecrt_master_slave_config(mst, imp->alias, imp->position, imp->vender_id, imp->product_code)))
			{
				throw std::runtime_error("failed to slave config");
			}
			// Set Sdo
			for (auto &sdo : imp->sdo_vec)
			{
				switch (sdo->size_)
				{
				case 8:		ecrt_slave_config_sdo8(imp->ec_slave_config, sdo->index_, sdo->subindex_, sdo->sdo_data_uint8_); break;
				case 16:	ecrt_slave_config_sdo16(imp->ec_slave_config, sdo->index_, sdo->subindex_, sdo->sdo_data_uint16_); break;
				case 32:	ecrt_slave_config_sdo32(imp->ec_slave_config, sdo->index_, sdo->subindex_, sdo->sdo_data_uint32_); break;
				default:    throw std::runtime_error("invalid size of sdo, it must be 8, 16 or 32");
				}
			}

			// Configure the slave's PDOs and sync masters
			if (ecrt_slave_config_pdos(imp->ec_slave_config, 4, imp->ec_sync_info))throw std::runtime_error("failed to slave config pdos");

			// Configure the slave's domain
			if (ecrt_domain_reg_pdo_entry_list(imp->domain, imp->ec_pdo_entry_reg_vec.data()))throw std::runtime_error("failed domain_reg_pdo_entry");

			// Configure the slave's discrete clock			
			if (imp->distributed_clock)ecrt_slave_config_dc(imp->ec_slave_config, *imp->distributed_clock.get(), 1000000, 4400000, 0, 0);
#endif
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::int8_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::int16_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::int32_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::uint8_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::uint16_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::readPdo(int pdoGroupID, int pdoID, std::uint32_t &value) const->void
		{
			if (!imp->pdo_group_vec[pdoGroupID].is_tx) throw std::runtime_error("you can't read pdo in rx pdo");
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->readPdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::int8_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::int16_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::int32_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::uint8_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::uint16_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::writePdo(int pdoGroupID, int pdoID, std::uint32_t value)->void
		{
			imp->pdo_group_vec[pdoGroupID].pdo_vec[pdoID]->writePdo(value);
		}
		auto EthercatSlave::readSdo(int sdoID, std::int8_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::readSdo(int sdoID, std::int16_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::readSdo(int sdoID, std::int32_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::readSdo(int sdoID, std::uint8_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::readSdo(int sdoID, std::uint16_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::readSdo(int sdoID, std::uint32_t &value) const->void { imp->sdo_vec[sdoID]->readSdo(value); }
		auto EthercatSlave::writeSdo(int sdoID, std::int8_t value)->void{imp->sdo_vec[sdoID]->writeSdo(value);}
		auto EthercatSlave::writeSdo(int sdoID, std::int16_t value)->void { imp->sdo_vec[sdoID]->writeSdo(value); }
		auto EthercatSlave::writeSdo(int sdoID, std::int32_t value)->void { imp->sdo_vec[sdoID]->writeSdo(value); }
		auto EthercatSlave::writeSdo(int sdoID, std::uint8_t value)->void { imp->sdo_vec[sdoID]->writeSdo(value); }
		auto EthercatSlave::writeSdo(int sdoID, std::uint16_t value)->void { imp->sdo_vec[sdoID]->writeSdo(value); }
		auto EthercatSlave::writeSdo(int sdoID, std::uint32_t value)->void { imp->sdo_vec[sdoID]->writeSdo(value); }
		
		EthercatMaster::EthercatMaster():imp(new Imp){}
		EthercatMaster::~EthercatMaster(){}
		auto EthercatMaster::instance()->EthercatMaster&
		{
			if (!instancePtr())throw std::runtime_error("please first create an instance fo EthercatMaster");
			return *instancePtr().get();
		};
		auto EthercatMaster::instancePtr()->const std::unique_ptr<EthercatMaster> &
		{
			static std::unique_ptr<EthercatMaster> instance_ptr;
			return std::ref(instance_ptr);
		};
		auto EthercatMaster::loadXml(const Aris::Core::XmlElement &xml_ele)->void
		{
			/*Load EtherCat slave types*/
			std::map<std::string, const Aris::Core::XmlElement *> slaveTypeMap;
			
			auto pSlaveTypes = xml_ele.FirstChildElement("SlaveType");
			for (auto pType = pSlaveTypes->FirstChildElement(); pType != nullptr; pType = pType->NextSiblingElement())
			{
				slaveTypeMap.insert(std::make_pair(std::string(pType->name()), pType));
			}
			
			/*Load all slaves*/
			auto pSlaves = xml_ele.FirstChildElement("Slave");
			for (auto pSla = pSlaves->FirstChildElement(); pSla != nullptr; pSla = pSla->NextSiblingElement())
			{
				this->addSlave<EthercatSlave>(std::ref(*slaveTypeMap.at(std::string(pSla->Attribute("type")))));
			}
		}
		auto EthercatMaster::start()->void
		{
			if (imp->is_running_)throw std::runtime_error("master already running");
			imp->is_running_ = true;

			// init begin
#ifdef UNIX
			if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) { throw std::runtime_error("lock failed"); }

			imp->ec_master = ecrt_request_master(0);
			if (!imp->ec_master) { throw std::runtime_error("master request failed!"); }

			for (size_t i = 0; i < imp->slave_vec.size(); ++i)
			{
				imp->slave_vec[i]->imp->position = static_cast<std::uint16_t>(i);
				imp->slave_vec[i]->init();
			}

			ecrt_master_activate(imp->ec_master);

			for (auto &sla : imp->slave_vec)
			{
				sla->imp->domain_pd = ecrt_domain_data(sla->imp->domain);
			}
			//init end

			rt_print_auto_init(1);
			const int priority = 99;

			rt_task_create(&imp->rt_task, "realtime core", 0, priority, T_FPU);
			rt_task_start(&imp->rt_task, &EthercatMaster::Imp::rt_task_func, NULL);
#endif
		};
		auto EthercatMaster::stop()->void
		{
			if (!imp->is_running_)throw std::runtime_error("master is not running, so can't stop");

			imp->is_stopping_ = true;
#ifdef UNIX
			rt_task_delete(&imp->rt_task);

			ecrt_master_deactivate(imp->ec_master);
			ecrt_release_master(imp->ec_master);
#endif
			imp->is_stopping_ = false;
			imp->is_running_ = false;
		}
		auto EthercatMaster::addSlavePtr(EthercatSlave *pSla)->void
		{
			imp->slave_vec.push_back(std::unique_ptr<EthercatSlave>(pSla));
		}
	}
}
