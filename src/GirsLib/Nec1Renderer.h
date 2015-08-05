//{38.4k,564}<1,-1|1,-3>(16,-8,D:8,S:8,F:8,~F:8,1,^108m,(16,-4,1,^108m)*) [D:0..255,S:0..255=255-D,F:0..255]

#ifndef NEC1RENDERER_H
#define	NEC1RENDERER_H

#include <Arduino.h>
#include "Renderer.h"
#include "IrSignal.h"

class Nec1Renderer : public Renderer {
public:
    static const frequency_t frequency = 38400U;
    static const uint16_t introLength = 68U;
    static const uint16_t repeatLength = 4U;
    static const uint16_t endingLength = 0U;
    Nec1Renderer(unsigned int D, unsigned int S, unsigned int F) { init(D, S, F); }
    Nec1Renderer(unsigned int D, unsigned int F) { init(D, 255-D, F); };
    virtual ~Nec1Renderer();
    IrSignal* toSignal() const;

private:
    microseconds_t intro[introLength];
    static const microseconds_t repeat[repeatLength];
    void lsbByte(int& i, uint32_t& sum, unsigned int D);
    void process(int& i, uint32_t& sum, int data);
    void init(unsigned int D, unsigned int S, unsigned int F);

};

#endif	/* NEC1RENDERER_H */

