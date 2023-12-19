
#include "basecmd.h" // oid_alloc
//#include "board/gpio.h" // struct gpio_out
#include "board/irq.h" // irq_disable
#include "board/misc.h" // timer_is_before
#include "basecmd.h" // oid_alloc
#include "command.h" // DECL_COMMAND
#include "sched.h" // sched_add_timer
#include "spicmds.h" // spidev_transfer


struct valves_s {
    struct timer timer;   
    uint32_t rest_ticks;
    struct spidev_s *spi;
    struct move_queue_head mq;
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

    spidev_transfer(d->spi, 1, sizeof(m->values), m->values);
    move_free(m);

    return SF_DONE;
}

void
command_config_timed_spi(uint32_t *args)
{
    struct valves_s *ts = oid_alloc(args[0], command_config_timed_spi
                                   , sizeof(*ts));
    ts->timer.func = timed_spi_event;
    ts->spi = spidev_oid_lookup(args[1]);
    move_queue_setup(&ts->mq, sizeof(struct spi_move));
}
DECL_COMMAND(command_config_timed_spi,
             "config_timed_spi oid=%c spi_oid=%c");

void
command_queue_timed_spi(uint32_t *args)
{
    struct valves_s *d = oid_lookup(args[0], command_config_timed_spi);
    struct spi_move *m = move_alloc();
    uint32_t time = m->waketime = args[1];

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

/*void
command_query_timed_spi_status(uint32_t *args)
{
    struct valves_s *ax = oid_lookup(args[0], command_config_timed_spi);
    uint8_t msg[2] = { AR_FIFO_STATUS | AM_READ, 0x00 };
    uint32_t time1 = timer_read_time();
    spidev_transfer(ax->spi, 1, sizeof(msg), msg);
    uint32_t time2 = timer_read_time();
    adxl_status(ax, args[0], time1, time2, msg[1]);
}
DECL_COMMAND(command_query_adxl345_status, "query_adxl345_status oid=%c");


void
timed_spi_task(void)
{
    if (!sched_check_wake(&adxl345_wake))
        return;
    uint8_t oid;
    struct adxl345 *ax;
    foreach_oid(oid, ax, command_config_adxl345) {
        uint_fast8_t flags = ax->flags;
        if (!(flags & AX_PENDING))
            continue;
        if (flags & AX_HAVE_START)
            adxl_start(ax, oid);
        else
            adxl_query(ax, oid);
    }
}
DECL_TASK(adxl345_task);

*/