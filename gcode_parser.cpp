#include "gcode_parser.h"
#include "queue.h"
#include "motor.h"
#include "pinio.h"
#include "serial.h"

#define IS_DIGIT(c) (c >= '0' && c <= '9')
#define IS_LETTER(c) (c >= 'A' && c <= 'Z')
#define IS_WHITECHAR(c) (c == ' ' || c == '\t')
#define IS_ENDING(c) (c == '\n' || c == '\r')
#define ATOI(c)     (c - '0')

#define STATE_ERROR 1

GCODE_PARAM params[8];
uint8_t current_parameter;
uint8_t option_all_relative = 0;
TARGET next_target;

// Parser is implemented as a finite state automata (DFA)
// This is pointer holds function with actions expected for current progress, each of these functions
// represent one possible state
uint8_t (*current_state)(uint8_t c);

//a few state functions prototypes
uint8_t start_parsing_parameter(uint8_t );
uint8_t start_parsing_number(uint8_t);


/// convert a floating point input value into an integer with appropriate scaling.
/// \param mantissa the actual digits of our floating point number
/// \param exponent scale mantissa by \f$10^{-exponent}\f$
/// \param sign  positive or negative?
/// \param multiplicand multiply by this amount during conversion to integer
///
/// Tested for up to 42'000 mm (accurate), 420'000 mm (precision 10 um) and
/// 4'200'000 mm (precision 100 um).
static int32_t decfloat_to_int(uint32_t mantissa, uint8_t exponent, uint8_t sign, uint16_t multiplicand) {

    // exponent=1 means we've seen a decimal point but no digits after it, and e=2 means we've seen a decimal point with one digit so it's too high by one if not zero
    if (exponent)
        exponent--;

    // This raises range for mm by factor 1000 and for inches by factor 100.
    // It's a bit expensive, but we should have the time while parsing.
    while (exponent && multiplicand % 10 == 0) {
        multiplicand /= 10;
        exponent--;
    }

    mantissa *= multiplicand;
    if (exponent)
        mantissa = (mantissa + powers[exponent] / 2) / powers[exponent];

    return sign ? -(int32_t)mantissa : (int32_t)mantissa;
}

void parser_reset()
{
    uint8_t i;
   current_state = start_parsing_parameter; 
   current_parameter = 0;
   for(i = 0; i < 8; ++i)
   {
       //params[i].name = '\0';
       params[i].value = 0;
       params[i].exponent = 0;
       params[i].is_negative = 0;
    }
}

void parser_init()
{
#ifdef STEPS_PER_M_Z
    next_target.F = SEARCH_FEEDRATE_Z;
#else
    next_target.F = SEARCH_FEEDRATE_Y;    
#endif
#ifdef STEPS_PER_M_E
    next_target.e_multiplier = 256;
#endif
    next_target.f_multiplier = 256;
    parser_reset();
}

// This function executes all possible commands after those are parsed
// Tinker with this to add new commands 
uint8_t process_command()
{
    DDA *dda;
    uint8_t result = 0;
    if (option_all_relative) 
        next_target.axis[X] = next_target.axis[Y] = 0;
    for(int i = 1; i <= current_parameter; ++i)
    {
        switch(params[i].name)
        {
            case 'X':
                next_target.axis[X] = decfloat_to_int(params[i].value, params[i].exponent, params[i].is_negative, 1000);
            break;
            case 'Y':
                next_target.axis[Y] = decfloat_to_int(params[i].value, params[i].exponent, params[i].is_negative, 1000);
            break;
            case 'F':
                next_target.F = decfloat_to_int(params[i].value, params[i].exponent, params[i].is_negative, 1);
            break;
        }
    }
    // convert relative to absolute
    if (option_all_relative) {
        next_target.axis[X] += startpoint.axis[X];
        next_target.axis[Y] += startpoint.axis[Y];
    }
    // params[0] is always a operation with code
    switch(params[0].name)
    {
        case 'G':
            switch(params[0].value)
            {   
                case 1: 
                    //? Example: G1
                    //?
                    //? Linear move
                    enqueue(&next_target); break;
                case 28: 
                    //? Example: G28
                    //?
                    //? home axis, y only
                    next_target.axis[X] = next_target.axis[Y] = 0;
                    home_pos_y(); 
                    // just set X axis pos as zero
                    current_position.axis[X] = startpoint.axis[X] = 0;
                    dda_new_startpoint();
                break; 
                case 90: 
                    //? Example: G90
                    //?
                    //? Absolute positioning
                    option_all_relative = 0;
                break;
                case 91: 
                    //? Example: G91
                    //?
                    //? Relative positioning
                    option_all_relative = 1;
                break;
                default:
                    result = STATE_ERROR;
            }
        break;
        case 'M':
            switch(params[0].value)
            {
                case 0: 
                    //? Example: M0
                    //?
                    //? Stop or unconditional stop
                    ATOMIC_START
                    dda = queue_current_movement();
                    if (dda != NULL)
                    {
                        update_current_position();
                        memcpy(&startpoint, &current_position, sizeof(TARGET));
                        dda->live = 0; dda->done = 1;
                        #ifdef LOOKAHEAD
                        dda->id--;
                        #endif
                        queue_flush();
                        queue_step() ;
                        dda_new_startpoint();
                    }
                    ATOMIC_END
                break;
                
                case 112:
                    //? Example: M112
                    //?
                    //? Any moves in progress are immediately terminated, then the controller
                    //? shuts down. All motors are turned off. Only way to
                    //? restart is to press the reset button on the master microcontroller.
                    //? See also M0.
                    //?
                    timer_stop();
                    queue_flush();
                    cli();

                break;
                
                case 114:
                    //? Example: M114
                    //?
                    //? Get current pos
                update_current_position();
                sersendf_P(PSTR("X:%lq,Y:%lq,F:%lu\n"),
                        current_position.axis[X], current_position.axis[Y],
                                current_position.F);
                break;
                
                case 119: 
                    //? Example: M119
                    //?
                    //? Endstops status
                    //power_on();
                    endstops_on();
                    //delay_ms(10); // allow the signal to stabilize
                    #if defined(Y_MIN_PIN)
                    sersendf_P(PSTR("X:%d,Y:%d\n"), x_min(), y_min());
                    #endif
                    endstops_off();
                break;
                case 202: //set acceleration
                break;
                case 222: //set speed
                    
                break;
                default:
                    result = STATE_ERROR;
            }
        break;
        default:
            result = STATE_ERROR;
    }
    return result;
}


uint8_t gcode_syntax_error(uint8_t c)
{
    if IS_ENDING(c)
        parser_reset();
    return STATE_ERROR;
}

uint8_t start_parsing_parameter(uint8_t c)
{
    //ignore
    if IS_WHITECHAR(c)
        return 0;
    
    // uppercase
    if (c >= 'a' && c <= 'z')
        c &= ~32;

    if IS_LETTER(c)
    {
        params[current_parameter].name = c;
        current_state = start_parsing_number;
        return 0;
    }
    current_state = gcode_syntax_error;
    return STATE_ERROR;
}

uint8_t parse_digit(uint8_t c)
{
    //process digit
    if IS_DIGIT(c)
    {
        // this is simply mantissa = (mantissa * 10) + atoi(c) in different clothes
        params[current_parameter].value = (params[current_parameter].value << 3) +
                                (params[current_parameter].value << 1) + ATOI(c);
                                
        if(params[current_parameter].exponent)
            ++params[current_parameter].exponent;
        return 0;
    }
    
    //this digit is a end of parameter
    if IS_WHITECHAR(c)
    {
        current_state = start_parsing_parameter;
        ++current_parameter;
        return 0;
    }
    // all done, this digit is a end of instruction
    if(c == '\n' || c == '\r') {
        //process instruction
        c = process_command();
        parser_reset();
        return c;
    }
    if(c == '.')
    {
        params[current_parameter].exponent = 1;
        return 0;
    }
    
    current_state = gcode_syntax_error;
    return STATE_ERROR;
}

uint8_t start_parsing_number(uint8_t c)
{
    current_state = parse_digit;
    //negative number
    if(c == '-')
    {
        params[current_parameter].is_negative = 1;
        return 0;
    }
    if IS_DIGIT(c)
    {
        parse_digit(c);
        return 0;
    }
    current_state = gcode_syntax_error;
    return STATE_ERROR;
}

// parsing of new instruction starts from HERE
uint8_t gcode_parse_char(uint8_t c) {

    uint8_t result, checksum_char = c;
    result = current_state(c);
    
    if IS_ENDING(c)
    {
        if ( result == STATE_ERROR) //error
            return 2;
        return 1;
    }
    return 0;
}

/// find Y MIN endstop position
void home_pos_y() {
    #if defined Y_MIN_PIN
        TARGET t = startpoint;

    t.axis[Y] = -1000000;
    if (SEARCH_FAST > SEARCH_FEEDRATE_Y)
      t.F = SEARCH_FAST;
    else
      t.F = SEARCH_FEEDRATE_Y;
    enqueue_home(&t, 0x04, 1);

    if (SEARCH_FAST > SEARCH_FEEDRATE_Y) {
      t.axis[Y] = +1000000;
            t.F = SEARCH_FEEDRATE_Y;
      enqueue_home(&t, 0x04, 0);
    }

        // set Y home
        //queue_wait();
        #ifdef  Y_MIN
      startpoint.axis[Y] = next_target.axis[Y] = (int32_t)(Y_MIN * 1000.);
        #else
      startpoint.axis[Y] = next_target.axis[Y] = 0;
        #endif
        dda_new_startpoint();
    #endif
}