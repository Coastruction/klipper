# control of valves through SPI
#
# Copyright (C) 2023  Andries Koopmans <andries@coastruction.com>
#
# This file may be distributed under the terms of the GNU GPLv3 license.
import logging
from . import bus

class spi_valves:
    def __init__(self, config):
        self.spi = bus.MCU_SPI_from_config(
            config, 0, pin_option="latch_pin", default_speed=25000000) #changed enable_pin to latch_pin
        self.mcu = mcu = self.spi.get_mcu()
        self.oid = oid = mcu.create_oid()

        mcu.register_config_callback(self._build_config)
        
        self.printer = config.get_printer()
        self.name = config.get_name()
        cname = self.name.split()[-1]
        
        self.reactor = self.printer.get_reactor()
        
        ppins = self.printer.lookup_object("pins")
        oe_pin = config.get("enable_pin")
        self.enable_pin = ppins.setup_pin('digital_out', oe_pin)
        self.enable_pin.setup_max_duration(0.)
        self.enable_pin.setup_start_value(1, 1, False)

        self.advance = config.get("advance", 0)
        
        gcode = self.printer.lookup_object('gcode')
        gcode.register_command("VALVES_SET", self.cmd_VALVES_SET,
                                   desc=self.cmd_VALVES_SET_help)
        gcode.register_command("VALVES_ENABLE", self.cmd_VALVES_ENABLE,
                                   desc=self.cmd_VALVES_ENABLE_help)
        gcode.register_command("VALVES_DISABLE", self.cmd_VALVES_DISABLE,
                                   desc=self.cmd_VALVES_DISABLE_help)        
        gcode.register_command("VALVES_SET_ADVANCE", self.cmd_VALVES_SET_ADVANCE,
                                   desc=self.cmd_VALVES_SET_ADVANCE_help) 
        self.last_values = [0,0,0,0,0,0,0,0,0,0]

        

    def _build_config(self):
        self.mcu.add_config_cmd("config_timed_spi oid=%d spi_oid=%d advance=%i"
                            % (self.oid, self.spi.get_oid(), self.advance))
        cmdqueue = self.spi.get_command_queue()
        self.queue_spi_out_cmd = self.mcu.lookup_command(
            "queue_timed_spi oid=%c clock=%u b0=%c b1=%c b2=%c b3=%c b4=%c b5=%c b6=%c b7=%c b8=%c b9=%c b10=%c", cq=cmdqueue)
        self.set_advance_cmd = self.mcu.lookup_command(
            "config_advance_parameter oid=%c advance=%i", cq=cmdqueue)

    def cmd_SET_REGISTER(self, reg, value):
        self.spi.spi_send([reg, value])

    def set_valves(self, print_time, values):
        clock = self.mcu.print_time_to_clock(print_time)
        self.queue_spi_out_cmd.send([self.oid, clock, values[0], values[1], values[2], values[3],  values[4], values[5], values[6], values[7], values[8], values[9], values[10]],
                        reqclock=clock)

    def _set_spi(self, print_time, values, is_resend=False):
        if values == self.last_values:
            if not is_resend:
                return
        self.set_valves(print_time, values)
        self.last_values = values

    cmd_VALVES_SET_help = "Set the value of all valves"
    def cmd_VALVES_SET(self, gcmd):
        parameters = gcmd.get_command_parameters().copy()
        values = parameters.pop('VALUES', None)
        values = [int(i) for i in values.split(',')]
        if values == None:
            raise gcmd.error("No VALUES parameter found")
        if len(values) != 11:
            raise gcmd.error("Expected 11 values for VALUES parameter")
        if max(values) > 255:
            raise gcmd.error("All VALUES must be less than 256")
        
        toolhead = self.printer.lookup_object('toolhead')
        toolhead.register_lookahead_callback(
            lambda print_time: self._set_spi(print_time, values))
        
    cmd_VALVES_ENABLE_help = "Enables the valves"
    def cmd_VALVES_ENABLE(self, gcmd):
        measured_time = self.reactor.monotonic()
        print_time = self.spi.get_mcu().estimated_print_time(measured_time)
        #print_time = toolhead.get_last_move_time()
        self.enable_pin.set_digital(print_time+0.100, 0) #0.1 is now arbitrary. If this time is too short, the MCU will shut down
        # because probably it cannot make the deadline.
        
    cmd_VALVES_DISABLE_help = "Disables the valves"
    def cmd_VALVES_DISABLE(self, gcmd):
        measured_time = self.reactor.monotonic()
        print_time = self.spi.get_mcu().estimated_print_time(measured_time)
        #print_time = toolhead.get_last_move_time()
        self.enable_pin.set_digital(print_time+0.100, 1)

    cmd_VALVES_SET_ADVANCE_help = "Sets the advance time parameters, controlling how much later or earliers the valves should open"
    def cmd_VALVES_SET_ADVANCE(self, gcmd):
        parameters = gcmd.get_command_parameters().copy()
        advance_time = parameters.pop('TIME', None)
        if advance_time is None:
            raise gcmd.error("Invalid TIME value")
        self.advance = self.mcu.seconds_to_clock(int(advance_time)/1000.0)
        self.set_advance_cmd.send([self.oid, self.advance])

def load_config_prefix(config):
    return spi_valves(config)
