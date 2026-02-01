 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

enum { I_RING_SIZE = 4 };
static_assert (1 <= I_RING_SIZE);

 
struct I_ring
{
  int ir_data[I_RING_SIZE];
  int ir_default_val;
  unsigned int ir_front;
  unsigned int ir_back;
  bool ir_empty;
};
typedef struct I_ring I_ring;

void i_ring_init (I_ring *ir, int ir_default_val);
int i_ring_push (I_ring *ir, int val);
int i_ring_pop (I_ring *ir);
bool i_ring_empty (I_ring const *ir) _GL_ATTRIBUTE_PURE;
