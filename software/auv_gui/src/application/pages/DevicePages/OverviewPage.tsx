import {
  ChartContainer,
  LineChart,
  RealTimeDomain,
  TimeAxis,
  VerticalAxis,
  ZoomBrush
} from '@electricui/components-desktop-charts'

import { Card, Colors } from '@blueprintjs/core'
import { Composition } from 'atomic-layout'
import { IntervalRequester } from '@electricui/components-core'
import React from 'react'
import { RouteComponentProps } from '@reach/router'
import { HTMLTable } from '@blueprintjs/core'
import { Callout } from '@blueprintjs/core'
import { Statistic, Statistics } from '@electricui/components-desktop-blueprint'
import { StateIndicator } from 'src/application/components/StateIndication'
import { MessageDataSource } from '@electricui/core-timeseries'

const layoutDescription = `
  Title Info
  LoopChart TempChart
`

//Data Inputs

const loop_timeDS = new MessageDataSource('loop_time')
const sys_stateDS = new MessageDataSource<number>('system_state')
const internal_tempDS = new MessageDataSource('internal_temp')

const baseSpeed = 100

export const OverviewPage = (props: RouteComponentProps) => {
  return (
    <React.Fragment>

      <Composition areas={layoutDescription} gap={10} autoCols="1fr">
        {Areas => (
          <React.Fragment>
            <Areas.Title>
              <Card interactive>
                <h1>OceanAI Command Center</h1>
                <Callout title="MCU Health" intent="success">
                  Microcontroller is running nominally.
                </Callout>
                <Callout title="Nominal Loop Time" intent="warning">
                  Loop Times should be at least 100hz to ensure proper stepper
                  functionality
                </Callout>
                <HTMLTable striped interactive condensed>
                  <thead>
                    <tr>
                      <h3>System Information</h3>
                    </tr>
                  </thead>
                  <tbody>
                    <tr>
                      <td>Processor</td>
                      <td>MIMXRT1062</td>
                    </tr>
                    <tr>
                      <td>CPU</td>
                      <td>ARM Cortex M7</td>
                    </tr>
                    <tr>
                      <td>RAM</td>
                      <td>1024 KB</td>
                    </tr>
                    <tr>
                      <td>Flash</td>
                      <td>8 MB</td>
                    </tr>
                    <tr>
                      <td>External Memory</td>
                      <td>8 MB</td>
                    </tr>
                  </tbody>
                </HTMLTable>
              </Card>
            </Areas.Title>

            <Areas.Info>
              <Card interactive>
                <h2>System Status</h2>
                <br />
                <Statistics>
                  <Statistic
                    label="Loop Time (hz)"
                    accessor="loop_time"
                    color="#9f7ef7"
                  />
                  <Statistic
                    label="System State"
                    accessor="system_state"
                    color={Colors.BLUE5}
                  />
                  <Statistic
                    label="CPU Temperature (C)"
                    accessor="internal_temp"
                    color={Colors.RED5}
                    precision={1}
                  />
                </Statistics>
                <br />
                <br />
                <br />
                <Card interactive elevation={1}>
                  <h2>System State</h2>
                  <StateIndicator />
                </Card>
                <br />
              </Card>
            </Areas.Info>
            <Areas.LoopChart>
              <Card>
                <h2>Loop Time</h2>
                <ChartContainer>
                  <LineChart dataSource={loop_timeDS} />
                  <TimeAxis label="Time (s)"/>
                  <VerticalAxis/>
                  <RealTimeDomain window={10000} />
                  <ZoomBrush />
                </ChartContainer>
              </Card>
            </Areas.LoopChart>

            <Areas.TempChart>
              <Card>
                <h2>CPU Temperature</h2>
                <ChartContainer>
                  <LineChart dataSource={internal_tempDS} />
                  <TimeAxis label="Time (s)"/>
                  <VerticalAxis/>
                  <RealTimeDomain 
                    window={10000}/>
                  <ZoomBrush />
                </ChartContainer>
              </Card>
            </Areas.TempChart>
          </React.Fragment>
        )}
      </Composition>
    </React.Fragment>
  )
}
