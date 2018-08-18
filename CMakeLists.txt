
cmake_minimum_required(VERSION 3.11)

project (d1)

set (DPDK_HOME /data/dpdk/dpdk-stable-18.02.1)


set (SRC main.cc)


include_directories(${DPDK_HOME}/x86_64-native-linuxapp-gcc/include)
set (CMAKE_C_FLAGS ${CMAKE_C_FLAGS}  "-m64 -march=native -Wall -g")
set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}  "-m64 -march=native -Wall -g")


set (DPDK_LIBS
  rte_net rte_ethdev rte_distributor rte_kni rte_mbuf rte_pmd_ring rte_power
  rte_acl rte_eal rte_kvargs rte_mempool rte_mempool_ring rte_pmd_e1000 rte_pmd_virtio rte_ring
  rte_cfgfile rte_hash rte_lpm rte_meter rte_pmd_i40e rte_pmd_vmxnet3_uio rte_sched rte_timer
  rte_cmdline rte_ip_frag rte_pipeline rte_pmd_ixgbe rte_mempool_stack rte_port rte_table rte_pmd_bnxt
  rte_pmd_ena rte_pmd_nfp rte_bus_pci rte_pci rte_pmd_bond rte_bus_vdev
)

set (LIBS -L${DPDK_HOME}/build/lib	-Wl,--whole-archive
	-Wl,--start-group ${DPDK_LIBS} numa -Wl,--end-group
	pthread dl rt m
	-Wl,--no-whole-archive
)


add_executable (d1 ${SRC})
TARGET_LINK_LIBRARIES(d1 ${LIBS})

