# The data for the chart
set data0 {42 49 33 38 51 46 29 41 44 57 59 52 37
           34 51 56 56 60 70 76 63 67 75 64 51}
set data1  {50 55 47 34 42 49 63 62 73 59 56 50 64
            60 67 67 58 59 73 77 84 82 80 84 98}

# The labels for the bottom x axis. Note the "-" means a minor tick.
set label0  {"0\nJun 4" "-" "-" "3" "-" "-" "6" "-" 
    	     "-" "9" "-" "-" "12" "-" "-" "15" "-" "-" "18" "-" "-"
             "21" "-" "-" "0\nJun 5"}

# The labels for the top x axis. Note that "-" means a minor tick.
set label1  {"Jun 3\n12" "-" "-" "15" "-" "-" "18" 
    	     "-" "-" "21" "-" "-" "Jun 4\n0" "-" "-" "3" "-" "-" 
             "6" "-" "-" "9" "-" "-" "12"}

# Create a XYChart object of size 310 x 310 pixels
set chart [ns_chartdir create xy 310 310]

# Set the plotarea at (50 50) and of size 200 x 200 pixels
ns_chartdir setplotarea $chart 50 50 200 200

# Add a title to the primary (left) y axis
ns_chartdir yaxis $chart settitle "US Dollars"

# Set the tick length to -4 pixels (-ve means ticks inside the plot # area)
ns_chartdir yaxis $chart setticklength -4

# Add a title to the secondary (right) y axis
ns_chartdir yaxis2 $chart settitle "HK Dollars (1 USD  7.8 HKD)"

# Set the tick length to -4 pixels (-ve means ticks inside the plot # area)
ns_chartdir yaxis2 $chart setticklength -4

# Synchronize the y-axis such that y2  7.8 x y1
ns_chartdir yaxis $chart syncyaxis 7.8

# Add a title to the bottom x axis
ns_chartdir xaxis $chart settitle "Hong Kong Time"

# Set the x axis labels using the given labels
ns_chartdir xaxis $chart setlabels $label0

# Set the major tick length to -4 pixels and minor tick length to -2
# pixels (-ve means ticks inside the plot area)
ns_chartdir xaxis $chart setticklength -4 -2

# Add a title to the top x-axis
ns_chartdir xaxis2 $chart settitle "New York Time"

# Set the x-axis labels using the given labels
ns_chartdir xaxis2 $chart setlabels $label1

# Set the major tick length to -4 pixels and minor tick length to -2
# pixels (-ve means ticks inside the plot area)
ns_chartdir xaxis2 $chart setticklength -4 -2

# Add a line layer to the chart with a line width of 2 pixels
ns_chartdir layer $chart create line $data0 "Server Load" 
ns_chartdir layer $chart setlinewidth 0 2

# Add an area layer to the chart with no area boundary line
ns_chartdir layer $chart create area $data1 "Transaction"
ns_chartdir layer $chart setlinewidth 1 0

# output the chart in PNG format
ns_chartdir save $chart "dualxaxis.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart
