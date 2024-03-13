#include "basecmd.h" // oid_alloc
#include "board/irq.h" // irq_disable
#include "board/misc.h" // timer_is_before
#include "basecmd.h" // oid_alloc
#include "command.h" // DECL_COMMAND
#include "sched.h" // sched_add_timer
#include "board/gpio.h" // GPIO pins // not used yet, but will have to.
#include "spicmds.h" // spidev_transfer


struct valves_s {
    struct timer timer;   
    uint32_t rest_ticks;
    struct spidev_s *spi;
    struct move_queue_head mq;
    int32_t advance_t;
};

struct spi_move {
    struct move_node node;
    uint32_t waketime;
    uint8_t values[11];
};

static uint_fast8_t timed_spi_event(struct timer *timer)
{
    //get the valves object that the timer is contained in
    struct valves_s *d = container_of(timer, struct valves_s, timer);
    if (move_queue_empty(&d->mq))
        shutdown("Missed scheduling of next timed spi event"); // I don't understand this message
    
    struct move_node *mn = move_queue_pop(&d->mq);
    struct spi_move *m = container_of(mn, struct spi_move, node);
    output("TIMED_SPI+EVENT %u(%c,%c,%c)", timer_read_time(), m->values[0],m->values[1],m->values[2]);
    spidev_transfer(d->spi, 1, sizeof(m->values), m->values);
    move_free(m);

    //schedule the next event
    if (!move_queue_empty(&d->mq)) {
        
        struct move_node *nn = move_queue_first(&d->mq);
        uint32_t wake = container_of(nn, struct spi_move, node)->waketime;
        output("Scheduling next SPI: %u", wake);
        d->timer.waketime = wake;
        return SF_RESCHEDULE;
    }
    
    return SF_DONE;
}

void
command_config_timed_spi(uint32_t *args)
{
    struct valves_s *ts = oid_alloc(args[0], command_config_timed_spi
                                   , sizeof(*ts));
    ts->timer.func = timed_spi_event;
    ts->spi = spidev_oid_lookup(args[1]);
    ts->advance_t = args[2];
    move_queue_setup(&ts->mq, sizeof(struct spi_move));
}
DECL_COMMAND(command_config_timed_spi,
             "config_timed_spi oid=%c spi_oid=%c advance=%i");

// Return the 'struct valves_s' for a given stepper oid
static struct valves_s *
timed_spi_oid_lookup(uint8_t oid)
{
    return oid_lookup(oid, command_config_timed_spi);
}

// Sets the advance parameters, i.e. how much later or earlier a valve
// should be set to compensate for delays.
void
command_config_advance_parameter(uint32_t *args)
{
    struct valves_s *d = timed_spi_oid_lookup(args[0]);
    d->advance_t = args[1];
    output("advance: %i"
          , d->advance_t);
}
DECL_COMMAND(command_config_advance_parameter,
             "config_advance_parameter oid=%c advance=%i");


void
command_queue_timed_spi(uint32_t *args)
{
    struct valves_s *d = oid_lookup(args[0], command_config_timed_spi);
    struct spi_move *m = move_alloc();
    
    output("QUEUE_TIME_SPI clock: %u,(%c,%c,%c)", args[1], args[2], args[3], args[4]);
    uint32_t time = m->waketime = args[1] + d->advance_t;

    m->values[0] = args[2];
    m->values[1] = args[3];
    m->values[2] = args[4];
    m->values[3] = args[5];
    m->values[4] = args[6];
    m->values[5] = args[7];
    m->values[6] = args[8];
    m->values[7] = args[9];
    m->values[8] = args[10];
    m->values[9] = args[11];
    m->values[10] = args[12];

    irq_disable();
    int first_on_queue = move_queue_push(&m->node, &d->mq);
    if (!first_on_queue) {
        irq_enable();
        return;
    }

    sched_del_timer(&d->timer);
    d->timer.waketime = time;
    d->timer.func = timed_spi_event;
    sched_add_timer(&d->timer);
    
    irq_enable();
}
DECL_COMMAND(command_queue_timed_spi,
             "queue_timed_spi oid=%c clock=%u b0=%c b1=%c b2=%c b3=%c b4=%c b5=%c b6=%c b7=%c b8=%c b9=%c b10=%c");

