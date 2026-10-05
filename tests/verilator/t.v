module t(input clk, input [7:0] a, output reg [7:0] b);
  always @(posedge clk) b <= a + 1;
endmodule
