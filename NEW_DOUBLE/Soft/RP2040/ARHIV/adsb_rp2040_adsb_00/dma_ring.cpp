
#include "dma_ring.h"
#include "hardware/regs/dreq.h"
static void __isr __time_critical_func(dma_irq0_handler)();
static DmaRing *g_rings[12]={0};
bool dma_ring_init(DmaRing &r, PIO pio, uint sm, size_t words)
{
	if((words&(words-1))!=0) return false; 
	r.buf=(uint32_t*)malloc(words*4); 
	if(!r.buf) return false; 
	r.words=words; r.widx=0;
	r.ridx=0;
	r.pio=pio; 
	r.sm=sm;
	r.dma_ch=dma_claim_unused_channel(true); 
	dma_channel_config c=dma_channel_get_default_config(r.dma_ch); 
	channel_config_set_read_increment(&c,false); 
	channel_config_set_write_increment(&c,true);
	channel_config_set_dreq(&c, pio==pio0 ? (DREQ_PIO0_RX0+sm) : (DREQ_PIO1_RX0+sm)); 
	channel_config_set_transfer_data_size(&c,DMA_SIZE_32);
	dma_channel_configure(r.dma_ch,&c,r.buf,&r.pio->rxf[sm],r.words,false); 
	dma_channel_set_irq0_enabled(r.dma_ch,true);
	static bool irq_installed=false; 
	if(!irq_installed)
	{ 
		irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0_handler); 
		irq_set_enabled(DMA_IRQ_0,true);
		irq_installed=true;
	}
	g_rings[r.dma_ch]=&r; 
	dma_channel_start(r.dma_ch);
	return true; 
}

void dma_ring_deinit(DmaRing &r)
{ 
	if(r.dma_ch>=0)
	{ 
		dma_channel_abort(r.dma_ch);
		dma_channel_set_irq0_enabled(r.dma_ch,false);
		g_rings[r.dma_ch]=nullptr; 
		dma_channel_unclaim(r.dma_ch);
	} 
	if(r.buf)
	{ 
		free(r.buf);
		r.buf=nullptr; 
	} 
}

size_t dma_ring_available(const DmaRing &r)
{
	uint32_t w=r.widx;
	uint32_t rd=r.ridx; 
	return (w-rd)&(r.words-1);
} 

bool dma_ring_get_word(DmaRing &r, uint32_t &w)
{ 
	if(!dma_ring_available(r)) return false;
	w=r.buf[r.ridx]; 
	r.ridx=(r.ridx+1)&(r.words-1); 
	return true; 
}

static void __isr __time_critical_func(dma_irq0_handler)()
{ 
	uint32_t pending=dma_hw->ints0;
	for(int ch=0; ch<12; ch++) 
		if(pending&(1u<<ch))
		{ 
			auto *r=g_rings[ch];
			if(r)
			{
				r->widx=(r->widx + r->words) & (r->words-1); 
				dma_hw->ints0=(1u<<ch);
				dma_channel_set_trans_count(ch,r->words,false);
				dma_channel_start(ch);
			}
			else 
			{ 
				dma_hw->ints0=(1u<<ch);
			}
		} 
}
