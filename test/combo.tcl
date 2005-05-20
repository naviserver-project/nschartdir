set noValue [ns_chartdir novalue]

set data0 {50 58 70 65 59 63 63 75 82 67 57 46 35 27 37 31 32 25 26 23 17 29 43 30 
           35 22 21 13 25 26 34 39 25 38 32 36 42 53 48 60 65 70 68 65 71 62 75 86 
           84 93 91 82 83 96 84 92 105 113 107 109 99 107 115 128 129 138 143 147 
           135 122 113 123 130 145 157 172 183 196 186 183 193 195 201 201 196 207 
           211 218 231 245 257 262 252 240 232 224 219 }

set data1 {40 51 39 50 52 47 65 70 56 46 58 54 38 39 43 49 49 59 70 86 80 63 64 51 67 
           48 30 49 63 58 59 60 58 66 76 70 57 70 59 53 71 86 103 115 132 150 136 141 
           146 138 127 118 115 99 99 118 126 121 110 100 109 95 78 61 48 57 76 86 71 54 
           53 56 38 26 11 6 10 14 0 17 11 9 6 16 36 46 27 40 55 65 84 84 97 103 109 117 110 }

set data2 [list 40 $noValue $noValue $noValue 38 $noValue $noValue $noValue 45 \
           $noValue $noValue $noValue 74 $noValue $noValue $noValue 103 \
           $noValue $noValue $noValue 121 $noValue $noValue $noValue 112 \
           $noValue $noValue $noValue 98 $noValue $noValue $noValue 124 \
           $noValue $noValue $noValue 121 $noValue $noValue $noValue 126 \
           $noValue $noValue $noValue 125 $noValue $noValue $noValue 112 \
           $noValue $noValue $noValue 92 $noValue $noValue $noValue 85 \
           $noValue $noValue $noValue 62 $noValue $noValue $noValue 47 \
           $noValue $noValue $noValue 48 $noValue $noValue $noValue 27 \
           $noValue $noValue $noValue 1 $noValue $noValue $noValue 4 \
           $noValue $noValue $noValue 9 $noValue $noValue $noValue 29 \
           $noValue $noValue $noValue 52 $noValue $noValue $noValue 36]

set labels { 0 "" "" "" - "" "" "" - "" "" "" 3 "" "" "" - "" "" "" - "" "" "" 6 "" "" "" - 
             "" "" "" - "" "" "" 9 "" "" "" - "" "" "" - "" "" "" 12 "" "" "" - "" "" "" - 
             "" "" "" 15 "" "" "" - "" "" "" - "" "" "" 18 "" "" "" - "" "" "" - "" "" "" 21 
             "" "" "" - "" "" "" - "" "" "" 24 }

# Create a XYChart object of size 300 x 210 pixels
set chart [ns_chartdir create xy 300 210]

# Set the plot area at (45, 10) and of size 240 x 150 pixels
ns_chartdir setplotarea $chart 45 10 240 150

# Add a legend box at (75, 5) (top of plot area) using 8 pts Arial
# font. Set the background and border to Transparent.
ns_chartdir addlegend $chart 75 5 0 Transparent Transparent "" 8 TextColor

# Reserve 10% margin at the top of the plot area during auto-scaling to
# leave space for the legend box
ns_chartdir yaxis $chart setautoscale 0.1

# Add a title to the y axis
ns_chartdir yaxis $chart settitle "Throughput (Mbps)"

# Set the labels on the x axis
ns_chartdir xaxis $chart setlabels $labels

# Add a title to the x-axis
ns_chartdir xaxis $chart settitle "June 12, 2001"

# Use non-indent mode for the axis. In this mode, the first and last
# bar in a bar chart is only drawn in half
ns_chartdir xaxis $chart setindent 0

# Add a line chart layer using red (0xff8080) with a line width of 3
# pixels
ns_chartdir layer $chart create line $data0 "Link A" 0xff8080

# Add a 3D bar chart layer with depth of 2 pixels using green
# (0x80ff80) as both the fill color and line color
ns_chartdir layer $chart create bar $data2 "Link B" 0x80ff80 0x80ff80
ns_chartdir layer $chart setdatacombinemethod 1 Side
ns_chartdir layer $chart setdepth 1 2 2

# Because each data point is 15 minutes, but the bar layer has only 1
# point per hour, so for every 4 data points, only 1 point that has
# data. We will increase the bar width so that it will cover the
# neighbouring points that have no data. This is done by decreasing the
# bar gap to a negative value.
ns_chartdir layer $chart setbargap 1 -2

# Add an area chart layer using blue (0x8080ff) for area and line
# colors
ns_chartdir layer $chart create area $data1 "Link C" 0x8080ff 0x8080ff

# output the chart in PNG format
ns_chartdir save $chart "combo.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart
