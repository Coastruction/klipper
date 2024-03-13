# add-on for the homing of the Z axis to also start the screw conveyor
# concurrently
#
# Copyright (C) 2023  Andries Koopmans <andries@coastruction.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
import logging

class hopper_feeder:
    def __init__(self, config):
        self.printer = config.get_printer()
        self.name = config.get_name()
        cname = self.name.split()[-1]
        
        self.reactor = self.printer.get_reactor()
        
        ppins = self.printer.lookup_object("pins")
        feeder_pin = config.get("feeder_pin")
        self.feeder_pin = ppins.setup_pin('digital_out', feeder_pin)
        self.feeder_pin.setup_max_duration(0.)
        self.feeder_pin.setup_start_value(0, 0, False)

        self.stepper_name = "z"

        self.printer.register_event_handler("homing:homing_move_begin", self.handle_home_move_start)
        self.printer.register_event_handler("homing:homing_move_end", self.handle_home_move_end)        

        
    def handle_home_move_start(self, hmove):
        try:
            endstop_short_name = hmove.endstops[0][1]
            if endstop_short_name == self.stepper_name:
                toolhead = self.printer.lookup_object('toolhead')
                print_time = toolhead.get_last_move_time()
                self.feeder_pin.set_digital(print_time+0.4, 1)
        except IndexError as e:
            pass
            
    def handle_home_move_end(self, hmove):
        try:
            endstop_short_name = hmove.endstops[0][1]
            if endstop_short_name == self.stepper_name:
                toolhead = self.printer.lookup_object('toolhead')
                print_time = toolhead.get_last_move_time()
                self.feeder_pin.set_digital(print_time+0.4, 0)
        except IndexError as e:
            pass


def load_config_prefix(config):
    return hopper_feeder(config)
