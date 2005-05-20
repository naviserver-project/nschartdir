# The data for the area chart
set data0 {42 49 33 38 51 46 29 41 44 57 59 52 37
           34 51 56 56 60 70 76 63 67 75 64 51};
set data1 {50 55 47 34 42 49 63 62 73 59 56 50 64
           60 67 67 58 59 73 77 84 82 80 84 98};
set data2 {87 89 85 66 53 39 24 21 37 56 37 22 21
           33 13 17 4 23 16 25 9 10 5 7 6};
set labels {"0" "-" "2" "-" "4" "-" "6" "-" "8"
            "-" "10" "-" "12" "-" "14" "-" "16" "-" "18" "-" "20"
      	    "-" "22" "-" "24"};

# Create a XYChart object of size 500 x 300 pixels
set chart [ns_chartdir create xy 500 300]

# Set the plotarea at (90, 30) and of size 300 x 240 pixels.
ns_chartdir setplotarea $chart 90 30 300 240

# Add a legend box at (405, 100)
ns_chartdir addlegend $chart 405 100

# Add a title to the chart
ns_chartdir addtitle $chart "Daily System Load"

# Add a title to the y axis. Draw the title upright (font angle = 0)
ns_chartdir yaxis $chart settitle "Database\nQueries\n(per sec)"

# Set the labels on the x axis
ns_chartdir xaxis $chart setlabels $labels

# Add the three data sets to the area layer
ns_chartdir layer $chart create area $data0 "Server # 1"
ns_chartdir layer $chart dataset 0 $data1 "Server # 2"
ns_chartdir layer $chart dataset 0 $data2 "Server # 3"

# Draw the area layer in 3D
ns_chartdir layer $chart set3d 0

# output the chart in PNG format
ns_chartdir save $chart "3dstackarea.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart

