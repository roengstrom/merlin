@config @daemons @merlin @queryhandler @livestatus
Feature: A notification should always be handled by the owning node.
	The "results" of the notification should always propagate to peers
	and masters but never downwards (to pollers).

	The notification data should propagate to Naemon, to local node, peers
	and masters. Upon restart of Naemon the data should be persistent.

	Background: Set up naemon configuration

		Given I have naemon hostgroup objects
			| hostgroup_name | alias |
			| pollergroup    | PG    |
			| emptygroup     | EG    |
		And I have naemon host objects
			| use          | host_name | address   | contacts  | max_check_attempts | hostgroups  |
			| default-host | hostA     | 127.0.0.1 | myContact | 2                  | pollergroup |
			| default-host | hostB     | 127.0.0.2 | myContact | 2                  | pollergroup |
		And I have naemon service objects
			| use             | host_name | description |
			| default-service | hostA     | PONG        |
			| default-service | hostB     | PONG        |
		And I have naemon contact objects
			| use             | contact_name |
			| default-contact | myContact    |


	Scenario: Custom service notifications sent to Naemon should be executed
		on the master if the poller does not own the service. Information about
		the notification should be sent to peer but NOT to poller.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |

		When I send naemon command SEND_CUSTOM_SVC_NOTIFICATION;hostA;PONG;4;testCase;A little comment
		And I send naemon command SEND_CUSTOM_SVC_NOTIFICATION;hostB;PONG;4;testCase;A little comment

		Then the_peer received event NOTIFICATION
			| notification_type   | 1                |
			| service_description | PONG             |
			| ack_data            | A little comment |
			| ack_author          | testCase         |
		And the_poller should not receive NOTIFICATION


	Scenario: Custom host notifications sent to Naemon should be executed
		on the master if the poller does not own the service. Information about
		the notification should be sent to peer but NOT to poller.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |

		When I send naemon command SEND_CUSTOM_HOST_NOTIFICATION;hostA;4;testCase;A little comment
		And I send naemon command SEND_CUSTOM_HOST_NOTIFICATION;hostB;4;testCase;A little comment

		Then the_peer received event NOTIFICATION
			| notification_type   | 0                |
			| ack_data            | A little comment |
			| ack_author          | testCase         |
		And the_poller should not have received NOTIFICATION


	Scenario: Custom service notifications sent to Naemon should be blocked and
		sent to the poller instead if the poller is the owner of the service.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup   |
			| peer   | the_peer    | 4001 | ignore      |
			| poller | the_poller  | 4002 | pollergroup |

		When I send naemon command SEND_CUSTOM_SVC_NOTIFICATION;hostA;PONG;4;testCase;A little comment

		Then the_poller received event EXTERNAL_COMMAND


	Scenario: Custom host notifications sent to Naemon should be blocked and
		sent to the poller instead if the poller is the owner of the host.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup   |
			| peer   | the_peer    | 4001 | ignore      |
			| poller | the_poller  | 4002 | pollergroup |
		When I send naemon command SEND_CUSTOM_HOST_NOTIFICATION;hostA;4;testCase;A little comment

		Then the_poller received event EXTERNAL_COMMAND


	Scenario: Service notifications have been generated and a peer has handled
		them. We should receive information about them being handled, it should
		register in Naemon and persist through restart and also not propagate
		any further.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |
		And I should have 0 services objects matching last_notification > 0

		When the_peer sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostA  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
		And the_peer sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostB  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 services objects matching last_notification > 0
		And the_poller should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 2 services object matching last_notification > 0


	Scenario: Host notifications have been generated and a peer has handled
		them. We should receive information about them being handled, it should
		register in Naemon and persist through restart and also not propagate
		any further.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |
		And I should have 0 hosts objects matching last_notification > 0

		When the_peer sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostA  |
			| state               | 1      |
			| output              | Not OK |
		And the_peer sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostB  |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 hosts objects matching last_notification > 0
		And the_poller should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 2 hosts object matching last_notification > 0


	Scenario: Service notifications have been generated and a poller has handled
		them. We should receive information about them being handled, it should
		register in Naemon and persist through restart and also not propagate
		any further.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup   |
			| peer   | the_peer    | 4001 | ignore      |
			| poller | the_poller  | 4002 | pollergroup |
		And I should have 0 services objects matching last_notification > 0

		When the_poller sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostA  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
		And the_poller sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostB  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 services objects matching last_notification > 0
		And the_poller should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 2 services object matching last_notification > 0


	Scenario: Host notifications have been generated and a poller has handled
		them. We should receive information about them being handled, it should
		register in Naemon and persist through restart and also not propagate
		any further.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup   |
			| peer   | the_peer    | 4001 | ignore      |
			| poller | the_poller  | 4002 | pollergroup |
		And I should have 0 hosts objects matching last_notification > 0

		When the_poller sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostA  |
			| state               | 1      |
			| output              | Not OK |
		And the_poller sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostB  |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 hosts objects matching last_notification > 0
		And the_poller should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 2 hosts objects matching last_notification > 0


	Scenario: As a poller, when generating a service notification result it
		should propagate to peer and master. Also the results should persist
		in Naemon through a restart.
	
		Given I start naemon with merlin nodes connected
			| type   | name       | port | hostgroup   |
			| master | the_master | 4001 | ignore      |
			| peer   | the_peer   | 4002 | pollergroup |
		And I should have 0 services objects matching last_notification > 0
		And for 3 times I send naemon command PROCESS_SERVICE_CHECK_RESULT;hostB;PONG;1;Not OK

		Then the_peer received event NOTIFICATION
			| notification_type   | 1      |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
			| host_name           | hostB  |
		And the_master received event NOTIFICATION
			| notification_type   | 1      |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
			| host_name           | hostB  |
		And I should have 1 services object matching last_notification > 0

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds
		
		Then I should have 1 services object matching last_notification > 0
		And I should have 1 services object matching last_notification > 0
		And 1 notification for service PONG on host hostB was sent after check result


	Scenario: As a poller, when generating a host notification result it
		should propagate to peer and master. Also the results should persist
		in Naemon through a restart.
	
		Given I start naemon with merlin nodes connected
			| type   | name       | port | hostgroup   |
			| master | the_master | 4001 | ignore      |
			| peer   | the_peer   | 4002 | pollergroup |
		And I should have 0 hosts objects matching last_notification > 0
		And I send naemon command PROCESS_HOST_CHECK_RESULT;hostB;1;Not OK

		Then the_peer received event NOTIFICATION
			| notification_type   | 0      |
			| state               | 1      |
			| output              | Not OK |
			| host_name           | hostB  |
		And the_master received event NOTIFICATION
			| notification_type   | 0      |
			| state               | 1      |
			| output              | Not OK |
			| host_name           | hostB  |
		And I should have 1 hosts object matching last_notification > 0

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 1 hosts object matching last_notification > 0
		And I should have 1 hosts object matching last_notification > 0
		And 1 notification for host hostB was sent after check result


	Scenario: As a poller, when a peer sends service notification info it
		should not propagate any further.

		Given I start naemon with merlin nodes connected
			| type   | name       | port | hostgroup   |
			| master | the_master | 4001 | ignore      |
			| peer   | the_peer   | 4002 | pollergroup |
		And I should have 0 services objects matching last_notification > 0

		When the_peer sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostA  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
		And the_peer sends event NOTIFICATION
			| notification_type   | 1      |
			| host_name           | hostB  |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 services objects matching last_notification > 0
		And the_master should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION


	Scenario: As a poller, when a peer sends host notification info it
		should not propagate any further.

		Given I start naemon with merlin nodes connected
			| type   | name       | port | hostgroup   |
			| master | the_master | 4001 | ignore      |
			| peer   | the_peer   | 4002 | pollergroup |
		And I should have 0 hosts objects matching last_notification > 0

		When the_peer sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostA  |
			| state               | 1      |
			| output              | Not OK |
		And the_peer sends event NOTIFICATION
			| notification_type   | 0      |
			| host_name           | hostB  |
			| state               | 1      |
			| output              | Not OK |

		Then I should have 2 hosts objects matching last_notification > 0
		And the_master should not receive NOTIFICATION
		And the_peer should not receive NOTIFICATION


	Scenario: In a peered system, when sending passive check results for
		a service it should cause the owning node to notify and also
		let the peer know that it has notified but not to pollers.
		Information about last notification should persist through
		a Naemon restart.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |
		And I should have 0 services objects matching last_notification > 0

		When for 3 times I send naemon command PROCESS_SERVICE_CHECK_RESULT;hostB;PONG;1;Not OK

		Then the_peer received event NOTIFICATION
			| notification_type   | 1      |
			| service_description | PONG   |
			| state               | 1      |
			| output              | Not OK |
			| host_name           | hostB  |
		And the_poller should not receive NOTIFICATION
		And I should have 1 services object matching last_notification > 0

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 1 services object matching last_notification > 0
		And 1 notification for service PONG on host hostB was sent after check result
		
	
	Scenario: In a peered system, when sending passive check results for
		a host it should cause the owning node to notify and also
		let the peer know that it has notified but not to pollers.
		Information about last notification should persist through
		a Naemon restart.

		Given I start naemon with merlin nodes connected
			| type   | name        | port | hostgroup  |
			| peer   | the_peer    | 4001 | ignore     |
			| poller | the_poller  | 4002 | emptygroup |
		And I should have 0 services objects matching last_notification > 0

		When I send naemon command PROCESS_HOST_CHECK_RESULT;hostB;1;Not OK

		Then the_peer received event NOTIFICATION
			| notification_type   | 0      |
			| state               | 1      |
			| output              | Not OK |
		And I should have 1 hosts object matching last_notification > 0

		When I send naemon command RESTART_PROGRAM
		And I wait for 3 seconds

		Then I should have 1 hosts object matching last_notification > 0
		And 1 notification for host hostB was sent after check result

	Scenario: As a master with a non notifying poller we should send
		notification without storing it since we won't be sending a
		check result directly after.

		Given I start naemon with merlin nodes connected
			| type   | name         | port | hostgroup   | notifies |
			| peer   | the_peer     | 4001 | ignore      | ignore   |
			| poller | poller_one   | 4002 | pollergroup | no       |
			| poller | poller_two   | 4003 | pollergroup | no       |
			| poller | poller_three | 4004 | emptygroup  | no       |

		When poller_one sends event HOST_CHECK
			| name                  | hostB |
			| state.state_type      | 0     |
			| state.current_state   | 1     |
			| state.current_attempt | 1     |
		And poller_one sends event HOST_CHECK
			| name                  | hostB |
			| state.state_type      | 1     |
			| state.current_state   | 1     |
			| state.current_attempt | 2     |
		And poller_one sends event HOST_CHECK
			| name                  | hostB |
			| state.state_type      | 1     |
			| state.current_state   | 1     |
			| state.current_attempt | 3     |
		And I wait for 1 second

		Then no notification was held
		And 1 host notification was sent
			| parameter         | value    |
			| hostname          | hostB    |
		And the_peer received event NOTIFICATION
		And poller_one received event NOTIFICATION
		And poller_two received event NOTIFICATION
		And poller_three should not receive NOTIFICATION
