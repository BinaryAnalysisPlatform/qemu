#include "tracing.h"

Frame *frame_new_std(uint64_t addr, int vcpu_id) {
  Frame *frame = g_new(Frame, 1);
  frame__init(frame);

  StdFrame *sframe = g_new(StdFrame, 1);
  std_frame__init(sframe);
  frame->std_frame = sframe;

  sframe->address = addr;
  sframe->thread_id = vcpu_id;

  OperandValueList *ol_in = g_new(OperandValueList, 1);
  operand_value_list__init(ol_in);
  ol_in->n_elem = 0;
  sframe->operand_pre_list = ol_in;

  OperandValueList *ol_out = g_new(OperandValueList, 1);
  operand_value_list__init(ol_out);
  ol_out->n_elem = 0;
  sframe->operand_post_list = ol_out;
  return frame;
}

void frame_add_operand(Frame *frame, OperandInfo *oi) {
    OperandValueList *ol;
    if (oi->operand_usage->written) {
        ol = frame->std_frame->operand_post_list;
    } else {
        ol = frame->std_frame->operand_pre_list;
    }

    oi->taint_info = g_new(TaintInfo, 1);
    taint_info__init(oi->taint_info);
    oi->taint_info->no_taint = 1;
    oi->taint_info->has_no_taint = 1;

    ol->n_elem += 1;
    ol->elem = g_renew(OperandInfo *, ol->elem, ol->n_elem);
    ol->elem[ol->n_elem - 1] = oi;
}
